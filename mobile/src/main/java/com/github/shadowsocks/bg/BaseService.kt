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
import android.os.*
import android.text.TextUtils
import android.util.Base64
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.R
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.AclSyncJob
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.plugin.PluginOptions
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.*
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.File
import java.net.UnknownHostException
import java.security.MessageDigest
import java.util.*
import java.util.concurrent.TimeUnit
import kotlin.reflect.KClass

/**
 * This object uses WeakMap to simulate the effects of multi-inheritance.
 */
object BaseService {
    /**
     * IDLE state is only used by UI and will never be returned by BaseService.
     */
    const val IDLE = 0
    const val CONNECTING = 1
    const val CONNECTED = 2
    const val STOPPING = 3
    const val STOPPED = 4

    class Data internal constructor(private val service: Interface) {
        @Volatile var profile: Profile? = null
        @Volatile var state = STOPPED
        @Volatile var plugin = PluginOptions()
        @Volatile var pluginPath: String? = null
        var sslocalProcess: GuardedProcess? = null

        var timer: Timer? = null
        var trafficMonitorThread: TrafficMonitorThread? = null

        val callbacks = RemoteCallbackList<IShadowsocksServiceCallback>()
        val bandwidthListeners = HashSet<IBinder>() // the binder is the real identifier

        var notification: ServiceNotification? = null
        val closeReceiver = broadcastReceiver { _, intent ->
            when (intent.action) {
                Action.RELOAD -> service.forceLoad()
                else -> {
                    Toast.makeText(service as Context, R.string.stopping, Toast.LENGTH_SHORT).show()
                    service.stopRunner(true)
                }
            }
        }
        var closeReceiverRegistered = false

        private fun state() = state

        val binder = object : IShadowsocksService.Stub() {
            override fun getState(): Int = state()
            override fun getProfileName(): String = profile?.formattedName ?: "Idle"

            override fun registerCallback(cb: IShadowsocksServiceCallback) {
                callbacks.register(cb)
            }
            override fun startListeningForBandwidth(cb: IShadowsocksServiceCallback) {
                if (bandwidthListeners.add(cb.asBinder())) {
                    if (timer == null) {
                        val t = Timer(true)
                        t.schedule(object : TimerTask() {
                            override fun run() {
                                if (state == CONNECTED && TrafficMonitor.updateRate()) app.handler.post {
                                    if (bandwidthListeners.isNotEmpty()) {
                                        val txRate = TrafficMonitor.txRate
                                        val rxRate = TrafficMonitor.rxRate
                                        val txTotal = TrafficMonitor.txTotal
                                        val rxTotal = TrafficMonitor.rxTotal
                                        val n = callbacks.beginBroadcast()
                                        for (i in 0 until n) try {
                                            val item = callbacks.getBroadcastItem(i)
                                            if (bandwidthListeners.contains(item.asBinder()))
                                                item.trafficUpdated(profile!!.id, txRate, rxRate, txTotal, rxTotal)
                                        } catch (_: Exception) { }  // ignore
                                        callbacks.finishBroadcast()
                                    }
                                }
                            }
                        }, 1000, 1000)
                        timer = t
                    }
                    TrafficMonitor.updateRate()
                    if (state == CONNECTED) cb.trafficUpdated(profile!!.id,
                            TrafficMonitor.txRate, TrafficMonitor.rxRate,
                            TrafficMonitor.txTotal, TrafficMonitor.rxTotal)
                }
            }

            override fun stopListeningForBandwidth(cb: IShadowsocksServiceCallback) {
                if (bandwidthListeners.remove(cb.asBinder()) && bandwidthListeners.isEmpty()) {
                    timer!!.cancel()
                    timer = null
                }
            }

            override fun unregisterCallback(cb: IShadowsocksServiceCallback) {
                stopListeningForBandwidth(cb)   // saves an RPC, and safer
                callbacks.unregister(cb)
            }
        }

        internal fun updateTrafficTotal(tx: Long, rx: Long) {
            val profile = profile ?: return
            val p = ProfileManager.getProfile(profile.id) ?: return // profile may have host, etc. modified
            p.tx += tx
            p.rx += rx
            ProfileManager.updateProfile(p)
            app.handler.post {
                if (bandwidthListeners.isNotEmpty()) {
                    val n = callbacks.beginBroadcast()
                    for (i in 0 until n) {
                        try {
                            val item = callbacks.getBroadcastItem(i)
                            if (bandwidthListeners.contains(item.asBinder())) item.trafficPersisted(p.id)
                        } catch (_: Exception) { }  // ignore
                    }
                    callbacks.finishBroadcast()
                }
            }
        }

        internal fun buildShadowsocksConfig() {
            val profile = profile!!
            val config = JSONObject()
                    .put("server", profile.host)
                    .put("server_port", profile.remotePort)
                    .put("password", profile.password)
                    .put("method", profile.method)
            val pluginPath = pluginPath
            if (pluginPath != null) {
                val pluginCmd = arrayListOf(pluginPath)
                if (TcpFastOpen.sendEnabled) pluginCmd.add("--fast-open")
                config
                        .put("plugin", Commandline.toString(service.buildAdditionalArguments(pluginCmd)))
                        .put("plugin_opts", plugin.toString())
            }
            File(app.filesDir, "shadowsocks.json").bufferedWriter().use { it.write(config.toString()) }
        }
    }
    interface Interface {
        val tag: String

        fun onBind(intent: Intent): IBinder? = if (intent.action == Action.SERVICE) data.binder else null

        fun checkProfile(profile: Profile): Boolean =
                if (TextUtils.isEmpty(profile.host) || TextUtils.isEmpty(profile.password)) {
                    stopRunner(true, (this as Context).getString(R.string.proxy_empty))
                    false
                } else true
        fun forceLoad() {
            val p = app.currentProfile
                    ?: return stopRunner(true, (this as Context).getString(R.string.profile_empty))
            if (!checkProfile(p)) return
            val s = data.state
            when (s) {
                STOPPED -> startRunner()
                CONNECTED -> {
                    stopRunner(false)
                    startRunner()
                }
                else -> Log.w(tag, "Illegal state when invoking use: $s")
            }
        }

        fun buildAdditionalArguments(cmd: ArrayList<String>): ArrayList<String> = cmd

        fun startNativeProcesses() {
            val data = data
            data.buildShadowsocksConfig()
            val cmd = buildAdditionalArguments(arrayListOf(
                    File((this as Context).applicationInfo.nativeLibraryDir, Executable.SS_LOCAL).absolutePath,
                    "-u",
                    "-b", "127.0.0.1",
                    "-l", DataStore.portProxy.toString(),
                    "-t", "600",
                    "-c", "shadowsocks.json"))

            val acl = getAclFile()
            if (acl != null) {
                cmd += "--acl"
                cmd += acl.absolutePath
            }

            if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

            data.sslocalProcess = GuardedProcess(cmd).start()
        }

        fun getAclFile(): File? {
            val route = data.profile!!.route
            return if (route == Acl.ALL) null else
                Acl.getFile(if (route == Acl.CUSTOM_RULES) Acl.CUSTOM_RULES_FLATTENED else route)
        }

        fun getListFile(): File? {
            val aclFile = getAclFile() ?: return null
            val listFile = File(aclFile.absolutePath + ".lst")
            val list = StringBuilder()
            aclFile.bufferedReader().useLines {
                for (line in it) {
                    if (line.startsWith("^(.*\\.)?")) {
                        list.append(line.substring(8).replace("\\","").replace("$",""))
                        list.append("\n")
                    }
                }
            }
            listFile.bufferedWriter().use { it.write(list.toString()) }
            return listFile
        }

        fun createNotification(): ServiceNotification

        fun startRunner() {
            this as Context
            if (Build.VERSION.SDK_INT >= 26) startForegroundService(Intent(this, javaClass))
            else startService(Intent(this, javaClass))
        }

        fun killProcesses() {
            val data = data
            data.sslocalProcess?.destroy()
            data.sslocalProcess = null
        }

        fun stopRunner(stopService: Boolean, msg: String? = null) {
            // channge the state
            changeState(STOPPING)

            app.track(tag, "stop")

            killProcesses()

            // clean up recevier
            val data = data
            this as Service
            if (data.closeReceiverRegistered) {
                unregisterReceiver(data.closeReceiver)
                data.closeReceiverRegistered = false
            }

            data.notification?.destroy()
            data.notification = null

            // Make sure update total traffic when stopping the runner
            data.updateTrafficTotal(TrafficMonitor.txTotal, TrafficMonitor.rxTotal)

            TrafficMonitor.reset()
            data.trafficMonitorThread?.stopThread()
            data.trafficMonitorThread = null

            // change the state
            changeState(STOPPED, msg)

            // stop the service if nothing has bound to it
            if (stopService) stopSelf()

            data.profile = null
        }

        val data: Data get() = instances[this]!!

        fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
            val data = data
            if (data.state != STOPPED) return Service.START_NOT_STICKY
            val profile = app.currentProfile
            if (profile == null) {
                stopRunner(true)
                return Service.START_NOT_STICKY
            }
            data.profile = profile
            profile.name = profile.formattedName

            TrafficMonitor.reset()
            val thread = TrafficMonitorThread()
            thread.start()
            data.trafficMonitorThread = thread

            if (!data.closeReceiverRegistered) {
                // register close receiver
                val filter = IntentFilter()
                filter.addAction(Action.RELOAD)
                filter.addAction(Intent.ACTION_SHUTDOWN)
                filter.addAction(Action.CLOSE)
                (this as Context).registerReceiver(data.closeReceiver, filter)
                data.closeReceiverRegistered = true
            }

            data.notification = createNotification()
            app.track(tag, "start")

            changeState(CONNECTING)

            thread {
                this as Context
                try {
                    if (profile.host == "198.199.101.152") {
                        val client = OkHttpClient.Builder()
                                .dns {
                                    listOf((Dns.resolve(it, false) ?: throw UnknownHostException())
                                            .parseNumericAddress())
                                }
                                .connectTimeout(10, TimeUnit.SECONDS)
                                .writeTimeout(10, TimeUnit.SECONDS)
                                .readTimeout(30, TimeUnit.SECONDS)
                                .build()
                        val mdg = MessageDigest.getInstance("SHA-1")
                        mdg.update(app.info.signatures[0].toByteArray())
                        val requestBody = FormBody.Builder()
                                .add("sig", String(Base64.encode(mdg.digest(), 0)))
                                .build()
                        val request = Request.Builder()
                                .url(app.remoteConfig.getString("proxy_url"))
                                .post(requestBody)
                                .build()

                        val proxies = client.newCall(request).execute()
                                .body()!!.string().split('|').toMutableList()
                        proxies.shuffle()
                        val proxy = proxies.first().split(':')
                        profile.host = proxy[0].trim()
                        profile.remotePort = proxy[1].trim().toInt()
                        profile.password = proxy[2].trim()
                        profile.method = proxy[3].trim()
                    }

                    if (profile.route == Acl.CUSTOM_RULES)
                        Acl.save(Acl.CUSTOM_RULES_FLATTENED, Acl.customRules.flatten(10))

                    data.plugin = PluginConfiguration(profile.plugin ?: "").selectedOptions
                    data.pluginPath = PluginManager.init(data.plugin)

                    // Clean up
                    killProcesses()

                    if (!profile.host.isNumericAddress())
                        profile.host = Dns.resolve(profile.host, true) ?: throw UnknownHostException()

                    startNativeProcesses()

                    if (profile.route !in arrayOf(Acl.ALL, Acl.CUSTOM_RULES)) AclSyncJob.schedule(profile.route)

                    changeState(CONNECTED)
                } catch (_: UnknownHostException) {
                    stopRunner(true, getString(R.string.invalid_server))
                } catch (_: VpnService.NullConnectionException) {
                    stopRunner(true, getString(R.string.reboot_required))
                } catch (exc: Throwable) {
                    stopRunner(true, "${getString(R.string.service_failed)}: ${exc.message}")
                    app.track(exc)
                }
            }
            return Service.START_NOT_STICKY
        }

        fun changeState(s: Int, msg: String? = null) {
            val data = instances[this]!!
            if (data.state == s && msg == null) return
            if (data.callbacks.registeredCallbackCount > 0) app.handler.post {
                val n = data.callbacks.beginBroadcast()
                for (i in 0 until n) try {
                    data.callbacks.getBroadcastItem(i).stateChanged(s, data.binder.profileName, msg)
                } catch (_: Exception) { }  // ignore
                data.callbacks.finishBroadcast()
            }
            data.state = s
        }
    }

    private val instances = WeakHashMap<Interface, Data>()
    internal fun register(instance: Interface) = instances.put(instance, Data(instance))

    val usingVpnMode: Boolean get() = DataStore.serviceMode == Key.modeVpn
    val serviceClass: KClass<out Any> get() = when (DataStore.serviceMode) {
        Key.modeProxy -> ProxyService::class
        Key.modeVpn -> VpnService::class
        Key.modeTransproxy -> TransproxyService::class
        else -> throw UnknownError()
    }
}
