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
import com.github.shadowsocks.net.LocalDnsServer
import com.github.shadowsocks.net.Socks5Endpoint
import com.github.shadowsocks.net.Subnet
import com.github.shadowsocks.preference.DataStore
import java.net.InetSocketAddress
import java.util.*

object LocalDnsService {
    private val googleApisTester =
            "(^|\\.)googleapis(\\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?){1,2}\$".toRegex()
    private val chinaIpList by lazy {
        app.resources.openRawResource(R.raw.china_ip_list).bufferedReader()
                .lineSequence().map(Subnet.Companion::fromString).filterNotNull().toList()
    }

    private val servers = WeakHashMap<LocalDnsService.Interface, LocalDnsServer>()

    interface Interface : BaseService.Interface {
        override suspend fun startProcesses() {
            super.startProcesses()
            val data = data
            val profile = data.proxy!!.profile
            if (!profile.udpdns) servers[this] = LocalDnsServer(this::resolver,
                    Socks5Endpoint(profile.remoteDns.split(",").first(), 53),
                    DataStore.proxyAddress).apply {
                when (profile.route) {
                    Acl.BYPASS_CHN, Acl.BYPASS_LAN_CHN, Acl.GFWLIST, Acl.CUSTOM_RULES -> {
                        remoteDomainMatcher = googleApisTester
                        localIpMatcher = chinaIpList
                    }
                    Acl.CHINALIST -> { }
                    else -> forwardOnly = true
                }
                start(InetSocketAddress(DataStore.listenAddress, DataStore.portLocalDns))
            }
        }

        override suspend fun killProcesses() {
            servers.remove(this)?.shutdown()
            super.killProcesses()
        }
    }
}
