/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2019 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2019 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import android.content.Context
import android.util.Base64
import com.github.shadowsocks.Core
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.AclSyncer
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.parseNumericAddress
import com.github.shadowsocks.utils.signaturesCompat
import com.github.shadowsocks.utils.useCancellable
import com.google.firebase.remoteconfig.ktx.get
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException
import java.net.*
import java.security.MessageDigest
import org.json.JSONArray

/**
 * This class sets up environment for ss-local.
 */
class ProxyInstance(val profile: Profile, private val route: String = profile.route) {
    private var configFile: File? = null
    var trafficMonitor: TrafficMonitor? = null
    val plugin by lazy { PluginManager.init(PluginConfiguration(profile.plugin ?: "")) }
    private var scheduleConfigUpdate = false

    suspend fun init(service: BaseService.Interface) {
        if (profile.isSponsored) {
            scheduleConfigUpdate = true
            val mdg = MessageDigest.getInstance("SHA-1")
            mdg.update(Core.packageInfo.signaturesCompat.first().toByteArray())
            val (config, success) = RemoteConfig.fetch()
            scheduleConfigUpdate = !success
            val conn = withContext(Dispatchers.IO) {
                // Network.openConnection might use networking, see https://issuetracker.google.com/issues/135242093
                service.openConnection(URL(config["proxy_url"].asString())) as HttpURLConnection
            }
            conn.requestMethod = "POST"
            conn.doOutput = true

            val proxies = conn.useCancellable {
                try {
                    outputStream.bufferedWriter().use {
                        it.write("sig=" + Base64.encodeToString(mdg.digest(), Base64.DEFAULT))
                    }
                    inputStream.bufferedReader().readText()
                } catch (e: IOException) {
                    throw BaseService.ExpectedExceptionWrapper(e)
                }
            }.split('|').toMutableList()
            proxies.shuffle()
            val proxy = proxies.first().split(':')
            profile.host = proxy[0].trim()
            profile.remotePort = proxy[1].trim().toInt()
            profile.password = proxy[2].trim()
            profile.method = proxy[3].trim()
        }

        // it's hard to resolve DNS on a specific interface so we'll do it here
        if (profile.host.parseNumericAddress() == null) {
            profile.host = try {
                service.resolver(profile.host).firstOrNull()
            } catch (e: IOException) {
                throw UnknownHostException().initCause(e)
            }?.hostAddress ?: throw UnknownHostException()
        }
    }

    /**
     * Sensitive shadowsocks configuration file requires extra protection. It may be stored in encrypted storage or
     * device storage, depending on which is currently available.
     */
    fun start(service: BaseService.Interface, stat: File, configFile: File, extraFlag: String? = null,
              dnsRelay: Boolean = true) {
        trafficMonitor = TrafficMonitor(stat)

        this.configFile = configFile
        val config = profile.toJson()
        plugin?.let { (path, opts, isV2) ->
            if (service.isVpnService) {
                if (isV2) opts["__android_vpn"] = "" else config.put("plugin_args", JSONArray(arrayOf("-V")))
            }
            config.put("plugin", path).put("plugin_opts", opts.toString())
        }
        config.put("local_address", DataStore.listenAddress)
        config.put("local_port", DataStore.portProxy)
        configFile.writeText(config.toString())

        val cmd = arrayListOf(
                File((service as Context).applicationInfo.nativeLibraryDir, Executable.SS_LOCAL).absolutePath,
                "--stat-path", stat.absolutePath,
                "-c", configFile.absolutePath)
        if (service.isVpnService) cmd += arrayListOf("--vpn")
        if (extraFlag != null) cmd.add(extraFlag)
        if (dnsRelay) try {
            URI("dns://${profile.remoteDns}")
        } catch (e: URISyntaxException) {
            throw BaseService.ExpectedExceptionWrapper(e)
        }.let { dns ->
            cmd += arrayListOf(
                    "--dns-relay", "${DataStore.listenAddress}:${DataStore.portLocalDns}",
                    "--remote-dns", "${dns.host ?: "0.0.0.0"}:${if (dns.port < 0) 53 else dns.port}")
        }

        if (route != Acl.ALL) {
            cmd += "--acl"
            cmd += Acl.getFile(route).absolutePath
        }

        service.data.processes!!.start(cmd)
    }

    fun scheduleUpdate() {
        if (route !in arrayOf(Acl.ALL, Acl.CUSTOM_RULES)) AclSyncer.schedule(route)
        if (scheduleConfigUpdate) RemoteConfig.fetchAsync()
    }

    fun shutdown(scope: CoroutineScope) {
        trafficMonitor?.apply {
            thread.shutdown(scope)
            persistStats(profile.id)    // Make sure update total traffic when stopping the runner
        }
        trafficMonitor = null
        configFile?.delete()    // remove old config possibly in device storage
        configFile = null
    }
}
