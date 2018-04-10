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

import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.parseNumericAddress
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.Inet6Address

object LocalDnsService {
    interface Interface : BaseService.Interface {
        override fun startNativeProcesses() {
            super.startNativeProcesses()
            val data = data
            val profile = data.profile!!

            fun makeDns(name: String, address: String, timeout: Int, edns: Boolean = true): JSONObject {
                val dns = JSONObject()
                .put("Name", name)
                .put("Address", when (address.parseNumericAddress()) {
                    is Inet6Address -> "[$address]"
                    else -> address
                })
                .put("Timeout", timeout)
                .put("EDNSClientSubnet", JSONObject().put("Policy", "disable"))
                if (edns) dns
                .put("Protocol", "tcp")
                .put("Socks5Address", "127.0.0.1:" + DataStore.portProxy)
                else dns.put("Protocol", "udp")

                return dns
            }

            fun buildOvertureConfig(file: String): String {
                val config = JSONObject()
                        .put("BindAddress", "127.0.0.1:" + DataStore.portLocalDns)
                        .put("RedirectIPv6Record", true)
                        .put("DomainBase64Decode", false)
                        .put("HostsFile", "hosts")
                        .put("MinimumTTL", 120)
                        .put("CacheSize", 4096)
                val remoteDns = JSONArray(profile.remoteDns.split(",")
                        .mapIndexed { i, dns -> makeDns("UserDef-$i", dns.trim() + ":53", 9) })
                val localDns = JSONArray(arrayOf(
                        makeDns("Primary-1", "208.67.222.222:443", 3, false),
                        makeDns("Primary-2", "119.29.29.29:53", 3, false),
                        makeDns("Primary-3", "114.114.114.114:53", 3, false)
                ))

                when (profile.route) {
                    Acl.BYPASS_CHN, Acl.BYPASS_LAN_CHN, Acl.GFWLIST, Acl.CUSTOM_RULES -> config
                            .put("PrimaryDNS", localDns)
                            .put("AlternativeDNS", remoteDns)
                            .put("IPNetworkFile", "china_ip_list.txt")
                    Acl.CHINALIST -> config
                            .put("PrimaryDNS", localDns)
                            .put("AlternativeDNS", remoteDns)
                    else -> config
                            .put("PrimaryDNS", remoteDns)
                            // no need to setup AlternativeDNS in Acl.ALL/BYPASS_LAN mode
                            .put("OnlyPrimaryDNS", true)
                }
                File(app.deviceContext.filesDir, file).writeText(config.toString())
                return file
            }

            if (!profile.udpdns) data.processes.start(buildAdditionalArguments(arrayListOf(
                    File(app.applicationInfo.nativeLibraryDir, Executable.OVERTURE).absolutePath,
                    "-c", buildOvertureConfig("overture.conf")
            )))
        }
    }
}
