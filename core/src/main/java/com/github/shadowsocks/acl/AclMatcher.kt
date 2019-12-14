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

package com.github.shadowsocks.acl

import com.github.shadowsocks.net.Subnet
import com.google.re2j.Pattern
import java.net.Inet4Address
import java.net.Inet6Address

class AclMatcher(id: String) {
    private val subnetsIpv4: List<Subnet.Immutable>
    private val subnetsIpv6: List<Subnet.Immutable>
    private val bypassDomains: Pattern?
    private val proxyDomains: Pattern?
    private val bypass: Boolean

    init {
        val bypassBuilder = StringBuilder()
        val proxyBuilder = StringBuilder()
        val (bypass, subnets) = Acl.parse(Acl.getFile(id).bufferedReader(), {
            if (bypassBuilder.isNotEmpty()) bypassBuilder.append('|')
            bypassBuilder.append(it)
        }, {
            if (proxyBuilder.isNotEmpty()) proxyBuilder.append('|')
            proxyBuilder.append(it)
        })
        subnetsIpv4 = subnets.filter { it.address is Inet4Address }.map { it.toImmutable() }
        subnetsIpv6 = subnets.filter { it.address is Inet6Address }.map { it.toImmutable() }
        bypassDomains = if (bypassBuilder.isEmpty()) null else Pattern.compile(bypassBuilder.toString())
        proxyDomains = if (proxyBuilder.isEmpty()) null else Pattern.compile(proxyBuilder.toString())
        this.bypass = bypass
    }

    fun shouldBypassIpv4(ip: ByteArray) = bypass xor subnetsIpv4.any { it.matches(ip) }
    fun shouldBypassIpv6(ip: ByteArray) = bypass xor subnetsIpv6.any { it.matches(ip) }
    fun shouldBypass(host: String): Boolean? {
        if (bypassDomains?.matches(host) == true) return true
        if (proxyDomains?.matches(host) == false) return false
        return null
    }
}
