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

import com.github.shadowsocks.Core.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.core.R
import com.github.shadowsocks.net.HostsFile
import com.github.shadowsocks.net.LocalDnsServer
import com.github.shadowsocks.net.Socks5Endpoint
import com.github.shadowsocks.net.Subnet
import com.github.shadowsocks.preference.DataStore
import kotlinx.coroutines.CoroutineScope
import java.net.InetSocketAddress
import java.net.URI
import java.net.URISyntaxException
import java.util.*

object LocalDnsService {
    private val googleApisTester =
            "(^|\\.)googleapis(\\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?){1,2}\$".toRegex()
    private val chinaIpList by lazy {
        app.resources.openRawResource(R.raw.china_ip_list).bufferedReader()
                .lineSequence().map(Subnet.Companion::fromString).filterNotNull().toList()
    }

    private val servers = WeakHashMap<Interface, LocalDnsServer>()

    interface Interface : BaseService.Interface {
        override suspend fun startProcesses(hosts: HostsFile) {
            super.startProcesses(hosts)
            val profile = data.proxy!!.profile
            val dns = try {
                URI("dns://${profile.remoteDns}")
            } catch (e: URISyntaxException) {
                throw BaseService.ExpectedExceptionWrapper(e)
            }
            LocalDnsServer(this::resolver,
                    Socks5Endpoint(dns.host, if (dns.port < 0) 53 else dns.port),
                    DataStore.proxyAddress,
                    hosts).apply {
                tcp = !profile.udpdns
                when (profile.route) {
                    Acl.BYPASS_CHN, Acl.BYPASS_LAN_CHN, Acl.GFWLIST, Acl.CUSTOM_RULES -> {
                        remoteDomainMatcher = googleApisTester
                        localIpMatcher = chinaIpList
                    }
                    Acl.CHINALIST -> { }
                    else -> forwardOnly = true
                }
            }.also { servers[this] = it }.start(InetSocketAddress(DataStore.listenAddress, DataStore.portLocalDns))
        }

        override fun killProcesses(scope: CoroutineScope) {
            servers.remove(this)?.shutdown(scope)
            super.killProcesses(scope)
        }
    }
}
