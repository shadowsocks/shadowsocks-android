/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.bg

import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.IBinder
import android.os.RemoteCallbackList
import android.os.RemoteException
import com.github.shadowsocks.BootReceiver
import com.github.shadowsocks.Core
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.core.R
import com.github.shadowsocks.net.DnsResolverCompat
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.broadcastReceiver
import com.github.shadowsocks.utils.readableMessage
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.analytics.ktx.analytics
import com.google.firebase.analytics.ktx.logEvent
import com.google.firebase.ktx.Firebase
import kotlinx.coroutines.*
import timber.log.Timber
import java.io.File
import java.io.IOException
import java.net.URL
import java.net.UnknownHostException

/**
 * This object uses WeakMap to simulate the effects of multi-inheritance.
 */
object BaseService {
    enum class State(val canStop: Boolean = false) {
        /**
         * Idle state is only used by UI and will never be returned by BaseService.
         */
        Idle,
        Connecting(true),
        Connected(true),
        Stopping,
        Stopped,
    }

    const val CONFIG_FILE = "shadowsocks.conf"
    const val CONFIG_FILE_UDP = "shadowsocks-udp.conf"

    interface ExpectedException
    class ExpectedExceptionWrapper(e: Exception) : Exception(e.localizedMessage, e), ExpectedException

    class Data internal constructor(private val service: Interface) {
        var state = State.Stopped
        var processes: GuardedProcessPool? = null
        var proxy: ProxyInstance? = null
        var udpFallback: ProxyInstance? = null
        var localDns: LocalDnsWorker? = null

        var notification: ServiceNotification? = null
        val closeReceiver = broadcastReceiver { _, intent ->
            when (intent.action) {
                Intent.ACTION_SHUTDOWN -> service.persistStats()
                Action.RELOAD -> service.forceLoad()
                else -> service.stopRunner()
            }
        }
        var closeReceiverRegistered = false

        val binder = Binder(this)
        var connectingJob: Job? = null

        fun changeState(s: State, msg: String? = null) {
            if (state == s && msg == null) return
            binder.stateChanged(s, msg)
            state = s
        }
    }

    class Binder(private var data: Data? = null) : IShadowsocksService.Stub(), CoroutineScope, AutoCloseable {
        private val callbacks = object : RemoteCallbackList<IShadowsocksServiceCallback>() {
            override fun onCallbackDied(callback: IShadowsocksServiceCallback?, cookie: Any?) {
                super.onCallbackDied(callback, cookie)
                stopListeningForBandwidth(callback ?: return)
            }
        }
        private val bandwidthListeners = mutableMapOf<IBinder, Long>()  // the binder is the real identifier
        override val coroutineContext = Dispatchers.Main.immediate + Job()
        private var looper: Job? = null

        override fun getState(): Int = (data?.state ?: State.Idle).ordinal
        override fun getProfileName(): String = data?.proxy?.profile?.name ?: "Idle"

        override fun registerCallback(cb: IShadowsocksServiceCallback) {
            callbacks.register(cb)
        }

        private fun broadcast(work: (IShadowsocksServiceCallback) -> Unit) {
            val count = callbacks.beginBroadcast()
            try {
                repeat(count) {
                    try {
                        work(callbacks.getBroadcastItem(it))
                    } catch (_: RemoteException) {
                    } catch (e: Exception) {
                        Timber.w(e)
                    }
                }
            } finally {
                callbacks.finishBroadcast()
            }
        }

        private suspend fun loop() {
            while (true) {
                delay(bandwidthListeners.values.minOrNull() ?: return)
                val proxies = listOfNotNull(data?.proxy, data?.udpFallback)
                val stats = proxies
                        .map { Pair(it.profile.id, it.trafficMonitor?.requestUpdate()) }
                        .filter { it.second != null }
                        .map { Triple(it.first, it.second!!.first, it.second!!.second) }
                if (stats.any { it.third } && data?.state == State.Connected && bandwidthListeners.isNotEmpty()) {
                    val sum = stats.fold(TrafficStats()) { a, b -> a + b.second }
                    broadcast { item ->
                        if (bandwidthListeners.contains(item.asBinder())) {
                            stats.forEach { (id, stats) -> item.trafficUpdated(id, stats) }
                            item.trafficUpdated(0, sum)
                        }
                    }
                }
            }
        }

        override fun startListeningForBandwidth(cb: IShadowsocksServiceCallback, timeout: Long) {
            launch {
                if (bandwidthListeners.isEmpty() and (bandwidthListeners.put(cb.asBinder(), timeout) == null)) {
                    check(looper == null)
                    looper = launch { loop() }
                }
                if (data?.state != State.Connected) return@launch
                var sum = TrafficStats()
                val data = data
                val proxy = data?.proxy ?: return@launch
                proxy.trafficMonitor?.out.also { stats ->
                    cb.trafficUpdated(proxy.profile.id, if (stats == null) sum else {
                        sum += stats
                        stats
                    })
                }
                data.udpFallback?.also { udpFallback ->
                    udpFallback.trafficMonitor?.out.also { stats ->
                        cb.trafficUpdated(udpFallback.profile.id, if (stats == null) TrafficStats() else {
                            sum += stats
                            stats
                        })
                    }
                }
                cb.trafficUpdated(0, sum)
            }
        }

        override fun stopListeningForBandwidth(cb: IShadowsocksServiceCallback) {
            launch {
                if (bandwidthListeners.remove(cb.asBinder()) != null && bandwidthListeners.isEmpty()) {
                    looper!!.cancel()
                    looper = null
                }
            }
        }

        override fun unregisterCallback(cb: IShadowsocksServiceCallback) {
            stopListeningForBandwidth(cb)   // saves an RPC, and safer
            callbacks.unregister(cb)
        }

        fun stateChanged(s: State, msg: String?) = launch {
            val profileName = profileName
            broadcast { it.stateChanged(s.ordinal, profileName, msg) }
        }

        fun trafficPersisted(ids: List<Long>) = launch {
            if (bandwidthListeners.isNotEmpty() && ids.isNotEmpty()) broadcast { item ->
                if (bandwidthListeners.contains(item.asBinder())) ids.forEach(item::trafficPersisted)
            }
        }

        override fun close() {
            callbacks.kill()
            cancel()
            data = null
        }
    }

    interface Interface {
        val data: Data
        val tag: String
        fun createNotification(profileName: String): ServiceNotification

        fun onBind(intent: Intent): IBinder? = if (intent.action == Action.SERVICE) data.binder else null

        fun forceLoad() {
            val (profile, fallback) = Core.currentProfile
                    ?: return stopRunner(false, (this as Context).getString(R.string.profile_empty))
            if (profile.host.isEmpty() || (!profile.method.equals("none") && profile.password.isEmpty()) ||
                    fallback != null && (fallback.host.isEmpty() || (!fallback.method.equals("none") && fallback.password.isEmpty()))) {
                stopRunner(false, (this as Context).getString(R.string.proxy_empty))
                return
            }
            val s = data.state
            when {
                s == State.Stopped -> startRunner()
                s.canStop -> stopRunner(true)
                else -> Timber.w("Illegal state $s when invoking use")
            }
        }

        val isVpnService get() = false

        suspend fun startProcesses() {
            val context = if (Build.VERSION.SDK_INT < 24 || Core.user.isUserUnlocked) app else Core.deviceStorage
            val configRoot = context.noBackupFilesDir
            val udpFallback = data.udpFallback
            data.proxy!!.start(this,
                    File(Core.deviceStorage.noBackupFilesDir, "stat_main"),
                    File(configRoot, CONFIG_FILE),
                    if (udpFallback == null && data.proxy?.plugin == null) "tcp_and_udp" else "tcp_only")
            if (udpFallback?.plugin != null) throw ExpectedExceptionWrapper(IllegalStateException(
                    "UDP fallback cannot have plugins"))
            udpFallback?.start(this,
                    File(Core.deviceStorage.noBackupFilesDir, "stat_udp"),
                    File(configRoot, CONFIG_FILE_UDP),
                    "udp_only", false)
            data.localDns = LocalDnsWorker(this::rawResolver).apply { start() }
        }

        fun startRunner() {
            this as Context
            if (Build.VERSION.SDK_INT >= 26) startForegroundService(Intent(this, javaClass))
            else startService(Intent(this, javaClass))
        }

        fun killProcesses(scope: CoroutineScope) {
            data.processes?.run {
                close(scope)
                data.processes = null
            }
            data.localDns?.shutdown(scope)
            data.localDns = null
        }

        fun stopRunner(restart: Boolean = false, msg: String? = null) {
            if (data.state == State.Stopping) return
            // channge the state
            data.changeState(State.Stopping)
            GlobalScope.launch(Dispatchers.Main.immediate) {
                Firebase.analytics.logEvent("stop") { param(FirebaseAnalytics.Param.METHOD, tag) }
                data.connectingJob?.cancelAndJoin() // ensure stop connecting first
                this@Interface as Service
                // we use a coroutineScope here to allow clean-up in parallel
                coroutineScope {
                    killProcesses(this)
                    // clean up receivers
                    val data = data
                    if (data.closeReceiverRegistered) {
                        unregisterReceiver(data.closeReceiver)
                        data.closeReceiverRegistered = false
                    }

                    data.notification?.destroy()
                    data.notification = null

                    val ids = listOfNotNull(data.proxy, data.udpFallback).map {
                        it.shutdown(this)
                        it.profile.id
                    }
                    data.proxy = null
                    data.udpFallback = null
                    data.binder.trafficPersisted(ids)
                }

                // change the state
                data.changeState(State.Stopped, msg)

                // stop the service if nothing has bound to it
                if (restart) startRunner() else {
                    BootReceiver.enabled = false
                    stopSelf()
                }
            }
        }

        fun persistStats() =
                listOfNotNull(data.proxy, data.udpFallback).forEach { it.trafficMonitor?.persistStats(it.profile.id) }

        suspend fun preInit() { }
        suspend fun resolver(host: String) = DnsResolverCompat.resolveOnActiveNetwork(host)
        suspend fun rawResolver(query: ByteArray) = DnsResolverCompat.resolveRawOnActiveNetwork(query)
        suspend fun openConnection(url: URL) = url.openConnection()

        fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
            val data = data
            if (data.state != State.Stopped) return Service.START_NOT_STICKY
            val expanded = Core.currentProfile
            this as Context
            if (expanded == null) {
                // gracefully shutdown: https://stackoverflow.com/q/47337857/2245107
                data.notification = createNotification("")
                stopRunner(false, getString(R.string.profile_empty))
                return Service.START_NOT_STICKY
            }
            val (profile, fallback) = expanded
            profile.name = profile.formattedName    // save name for later queries
            val proxy = ProxyInstance(profile)
            data.proxy = proxy
            data.udpFallback = if (fallback == null) null else ProxyInstance(fallback, profile.route)

            BootReceiver.enabled = DataStore.persistAcrossReboot
            if (!data.closeReceiverRegistered) {
                registerReceiver(data.closeReceiver, IntentFilter().apply {
                    addAction(Action.RELOAD)
                    addAction(Intent.ACTION_SHUTDOWN)
                    addAction(Action.CLOSE)
                }, "$packageName.SERVICE", null)
                data.closeReceiverRegistered = true
            }

            data.notification = createNotification(profile.formattedName)
            Firebase.analytics.logEvent("start") { param(FirebaseAnalytics.Param.METHOD, tag) }

            data.changeState(State.Connecting)
            data.connectingJob = GlobalScope.launch(Dispatchers.Main) {
                try {
                    Executable.killAll()    // clean up old processes
                    preInit()
                    proxy.init(this@Interface)
                    data.udpFallback?.init(this@Interface)
                    if (profile.route == Acl.CUSTOM_RULES) try {
                        withContext(Dispatchers.IO) {
                            Acl.customRules.flatten(10, this@Interface::openConnection).also {
                                Acl.save(Acl.CUSTOM_RULES, it)
                            }
                        }
                    } catch (e: IOException) {
                        throw ExpectedExceptionWrapper(e)
                    }

                    data.processes = GuardedProcessPool {
                        Timber.w(it)
                        stopRunner(false, it.readableMessage)
                    }
                    startProcesses()

                    proxy.scheduleUpdate()
                    data.udpFallback?.scheduleUpdate()

                    data.changeState(State.Connected)
                } catch (_: CancellationException) {
                    // if the job was cancelled, it is canceller's responsibility to call stopRunner
                } catch (_: UnknownHostException) {
                    stopRunner(false, getString(R.string.invalid_server))
                } catch (exc: Throwable) {
                    if (exc is ExpectedException) Timber.d(exc) else Timber.w(exc)
                    stopRunner(false, "${getString(R.string.service_failed)}: ${exc.readableMessage}")
                } finally {
                    data.connectingJob = null
                }
            }
            return Service.START_NOT_STICKY
        }
    }
}
