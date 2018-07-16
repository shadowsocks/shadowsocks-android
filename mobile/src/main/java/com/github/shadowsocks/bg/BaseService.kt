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

import android.annotation.TargetApi
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.IBinder
import android.os.RemoteCallbackList
import android.util.Base64
import android.util.Log
import androidx.core.os.UserManagerCompat
import androidx.core.os.bundleOf
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.R
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.AclSyncer
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.plugin.PluginOptions
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.*
import com.google.firebase.analytics.FirebaseAnalytics
import okhttp3.FormBody
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.net.InetAddress
import java.net.UnknownHostException
import java.security.MessageDigest
import java.util.*
import java.util.concurrent.TimeUnit

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

    const val CONFIG_FILE = "shadowsocks.conf"

    class Data internal constructor(private val service: Interface) {
        @Volatile var profile: Profile? = null
        @Volatile var state = STOPPED
        @Volatile var plugin = PluginOptions()
        @Volatile var pluginPath: String? = null
        val processes = GuardedProcessPool()

        var timer: Timer? = null
        var trafficMonitorThread: TrafficMonitorThread? = null

        val callbacks = RemoteCallbackList<IShadowsocksServiceCallback>()
        val bandwidthListeners = HashSet<IBinder>() // the binder is the real identifier

        var notification: ServiceNotification? = null
        val closeReceiver = broadcastReceiver { _, intent ->
            when (intent.action) {
                Action.RELOAD -> service.forceLoad()
                else -> service.stopRunner(true)
            }
        }
        var closeReceiverRegistered = false

        val binder = object : IShadowsocksService.Stub() {
            override fun getState(): Int = this@Data.state
            override fun getProfileName(): String = profile?.name ?: "Idle"

            override fun registerCallback(cb: IShadowsocksServiceCallback) {
                callbacks.register(cb)
            }
            override fun startListeningForBandwidth(cb: IShadowsocksServiceCallback) {
                if (bandwidthListeners.add(cb.asBinder())) {
                    if (timer == null) {
                        val t = Timer(true)
                        t.schedule(object : TimerTask() {
                            override fun run() {
                                val profile = profile ?: return
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
                                                item.trafficUpdated(profile.id, txRate, rxRate, txTotal, rxTotal)
                                        } catch (e: Exception) {
                                            printLog(e)
                                        }
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
            try {
                // this.profile may have host, etc. modified and thus a re-fetch is necessary (possible race condition)
                val profile = ProfileManager.getProfile((profile ?: return).id) ?: return
                profile.tx += tx
                profile.rx += rx
                ProfileManager.updateProfile(profile)
                app.handler.post {
                    if (bandwidthListeners.isNotEmpty()) {
                        val n = callbacks.beginBroadcast()
                        for (i in 0 until n) {
                            try {
                                val item = callbacks.getBroadcastItem(i)
                                if (bandwidthListeners.contains(item.asBinder())) item.trafficPersisted(profile.id)
                            } catch (e: Exception) {
                                printLog(e)
                            }
                        }
                        callbacks.finishBroadcast()
                    }
                }
            } catch (e: IOException) {
                if (!DataStore.directBootAware) throw e // we should only reach here because we're in direct boot
                val profile = DirectBoot.getDeviceProfile()!!
                profile.tx += tx
                profile.rx += rx
                profile.dirty = true
                DirectBoot.update(profile)
                DirectBoot.listenForUnlock()
            }
        }

        internal var shadowsocksConfigFile: File? = null
        internal fun buildShadowsocksConfig(): File {
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
            // sensitive Shadowsocks config is stored in
            val file = File(if (UserManagerCompat.isUserUnlocked(app)) app.filesDir else @TargetApi(24) {
                app.deviceContext.noBackupFilesDir  // only API 24+ will be in locked state
            }, CONFIG_FILE)
            shadowsocksConfigFile = file
            file.writeText(config.toString())
            return file
        }

        val aclFile: File? get() {
            val route = profile!!.route
            return if (route == Acl.ALL) null else Acl.getFile(route)
        }

        fun changeState(s: Int, msg: String? = null) {
            if (state == s && msg == null) return
            if (callbacks.registeredCallbackCount > 0) app.handler.post {
                val n = callbacks.beginBroadcast()
                for (i in 0 until n) try {
                    callbacks.getBroadcastItem(i).stateChanged(s, binder.profileName, msg)
                } catch (e: Exception) {
                    printLog(e)
                }
                callbacks.finishBroadcast()
            }
            state = s
        }
    }
    interface Interface {
        val tag: String

        fun onBind(intent: Intent): IBinder? = if (intent.action == Action.SERVICE) data.binder else null

        fun checkProfile(profile: Profile): Boolean =
                if (profile.host.isEmpty() || profile.password.isEmpty()) {
                    stopRunner(true, (this as Context).getString(R.string.proxy_empty))
                    false
                } else true
        fun forceLoad() {
            val profile = app.currentProfile
                    ?: return stopRunner(true, (this as Context).getString(R.string.profile_empty))
            if (!checkProfile(profile)) return
            val s = data.state
            when (s) {
                STOPPED -> startRunner()
                CONNECTED -> {
                    stopRunner(false)
                    startRunner()
                }
                else -> Crashlytics.log(Log.WARN, tag, "Illegal state when invoking use: $s")
            }
        }

        fun buildAdditionalArguments(cmd: ArrayList<String>): ArrayList<String> = cmd

        fun startNativeProcesses() {
            val data = data
            val profile = data.profile!!
            val cmd = buildAdditionalArguments(arrayListOf(
                    File((this as Context).applicationInfo.nativeLibraryDir, Executable.SS_LOCAL).absolutePath,
                    "-u",
                    "-b", "127.0.0.1",
                    "-l", DataStore.portProxy.toString(),
                    "-t", "600",
                    "-c", data.buildShadowsocksConfig().absolutePath))

            val acl = data.aclFile
            if (acl != null) {
                cmd += "--acl"
                cmd += acl.absolutePath
            }

            if (profile.udpdns) cmd += "-D"

            if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

            data.processes.start(cmd)
        }

        fun createNotification(profileName: String): ServiceNotification

        fun startRunner() {
            this as Context
            if (Build.VERSION.SDK_INT >= 26) startForegroundService(Intent(this, javaClass))
            else startService(Intent(this, javaClass))
        }

        fun killProcesses() = data.processes.killAll()

        fun stopRunner(stopService: Boolean, msg: String? = null) {
            // channge the state
            val data = data
            data.changeState(STOPPING)

            app.analytics.logEvent("stop", bundleOf(Pair(FirebaseAnalytics.Param.METHOD, tag)))

            killProcesses()

            // clean up recevier
            this as Service
            if (data.closeReceiverRegistered) {
                unregisterReceiver(data.closeReceiver)
                data.closeReceiverRegistered = false
            }

            data.shadowsocksConfigFile?.delete()    // remove old config possibly in device storage
            data.shadowsocksConfigFile = null

            data.notification?.destroy()
            data.notification = null

            // Make sure update total traffic when stopping the runner
            data.updateTrafficTotal(TrafficMonitor.txTotal, TrafficMonitor.rxTotal)

            TrafficMonitor.reset()
            data.trafficMonitorThread?.stopThread()
            data.trafficMonitorThread = null

            // change the state
            data.changeState(STOPPED, msg)

            // stop the service if nothing has bound to it
            if (stopService) stopSelf()

            data.profile = null
        }

        val data: Data get() = instances[this]!!

        fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
            val data = data
            if (data.state != STOPPED) return Service.START_NOT_STICKY
            val profile = app.currentProfile
            this as Context
            if (profile == null) {
                data.notification = createNotification("")  // gracefully shutdown: https://stackoverflow.com/questions/47337857/context-startforegroundservice-did-not-then-call-service-startforeground-eve
                stopRunner(true, getString(R.string.profile_empty))
                return Service.START_NOT_STICKY
            }
            profile.name = profile.formattedName    // save name for later queries
            data.profile = profile

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
                registerReceiver(data.closeReceiver, filter)
                data.closeReceiverRegistered = true
            }

            data.notification = createNotification(profile.formattedName)
            app.analytics.logEvent("start", bundleOf(Pair(FirebaseAnalytics.Param.METHOD, tag)))

            data.changeState(CONNECTING)

            thread("$tag-Connecting") {
                try {
                    if (profile.host == "198.199.101.152") {
                        val client = OkHttpClient.Builder()
                                .connectTimeout(10, TimeUnit.SECONDS)
                                .writeTimeout(10, TimeUnit.SECONDS)
                                .readTimeout(30, TimeUnit.SECONDS)
                                .build()
                        val mdg = MessageDigest.getInstance("SHA-1")
                        mdg.update(app.info.signaturesCompat.first().toByteArray())
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
                        Acl.save(Acl.CUSTOM_RULES, Acl.customRules.flatten(10))

                    data.plugin = PluginConfiguration(profile.plugin ?: "").selectedOptions
                    data.pluginPath = PluginManager.init(data.plugin)

                    // Clean up
                    killProcesses()

                    if (!profile.host.isNumericAddress()) {
                        thread("BaseService-resolve") {
                            profile.host = InetAddress.getByName(profile.host).hostAddress ?: ""
                        }.join(10 * 1000)
                        if (!profile.host.isNumericAddress()) throw UnknownHostException()
                    }

                    startNativeProcesses()

                    if (profile.route !in arrayOf(Acl.ALL, Acl.CUSTOM_RULES)) AclSyncer.schedule(profile.route)

                    data.changeState(CONNECTED)
                } catch (_: UnknownHostException) {
                    stopRunner(true, getString(R.string.invalid_server))
                } catch (_: VpnService.NullConnectionException) {
                    stopRunner(true, getString(R.string.reboot_required))
                } catch (exc: Throwable) {
                    stopRunner(true, "${getString(R.string.service_failed)}: ${exc.message}")
                    printLog(exc)
                }
            }
            return Service.START_NOT_STICKY
        }
    }

    private val instances = WeakHashMap<Interface, Data>()
    internal fun register(instance: Interface) = instances.put(instance, Data(instance))

    val usingVpnMode: Boolean get() = DataStore.serviceMode == Key.modeVpn
    val serviceClass get() = when (DataStore.serviceMode) {
        Key.modeProxy -> ProxyService::class
        Key.modeVpn -> VpnService::class
        Key.modeTransproxy -> TransproxyService::class
        else -> throw UnknownError()
    }
}
