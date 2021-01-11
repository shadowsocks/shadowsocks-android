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
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.AclSyncer
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.parseNumericAddress
import kotlinx.coroutines.CoroutineScope
import org.json.JSONArray
import java.io.File
import java.io.IOException
import java.net.URI
import java.net.URISyntaxException
import java.net.UnknownHostException

/**
 * This class sets up environment for ss-local.
 */
class ProxyInstance(val profile: Profile, private val route: String = profile.route) {
    private var configFile: File? = null
    var trafficMonitor: TrafficMonitor? = null
    val plugin by lazy { PluginManager.init(PluginConfiguration(profile.plugin ?: "")) }

    suspend fun init(service: BaseService.Interface) {
        // it's hard to resolve DNS on a specific interface so we'll do it here
        if (profile.host.parseNumericAddress() == null && profile.plugin != null) {
            profile.host = try {
                service.resolver(profile.host).firstOrNull()
            } catch (e: IOException) {
                throw UnknownHostException().initCause(e)
            }?.hostAddress ?: throw UnknownHostException()
        }
        // check the crypto
        val deprecatedCiphers
            = arrayOf("xchacha20-ietf-poly1305", "aes-192-gcm", "chacha20", "salsa20")
        for (c in deprecatedCiphers)
        {
            if (profile.method == c) {
                throw IllegalArgumentException("cipher $c is deprecated.")
            }
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
        config.put("udp_max_associations", 256);
        configFile.writeText(config.toString())

        val cmd = arrayListOf(
                File((service as Context).applicationInfo.nativeLibraryDir, Executable.SS_LOCAL).absolutePath,
                "--stat-path", stat.absolutePath,
                "-c", configFile.absolutePath,
                "--udp-bind-addr", "${DataStore.listenAddress}:${DataStore.portProxy}",
        )
        if (service.isVpnService) cmd += arrayListOf("--vpn")
        if (extraFlag != null) cmd.add(extraFlag)
        if (dnsRelay) try {
            URI("dns://${profile.remoteDns}")
        } catch (e: URISyntaxException) {
            throw BaseService.ExpectedExceptionWrapper(e)
        }.let { dns ->
            cmd += arrayListOf(
                    "--dns-addr", "${DataStore.listenAddress}:${DataStore.portLocalDns}",
                    "--local-dns-addr", "local_dns_path",
                    "--remote-dns-addr", "${dns.host ?: "0.0.0.0"}:${if (dns.port < 0) 53 else dns.port}")
        }

        if (route != Acl.ALL) {
            cmd += "--acl"
            cmd += Acl.getFile(route).absolutePath
        }

        service.data.processes!!.start(cmd)
    }

    fun scheduleUpdate() {
        if (route !in arrayOf(Acl.ALL, Acl.CUSTOM_RULES)) AclSyncer.schedule(route)
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
