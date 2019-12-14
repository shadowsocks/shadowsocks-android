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

import com.github.shadowsocks.utils.asIterable
import java.net.Inet4Address
import java.net.Inet6Address

class AclMatcher(acl: Acl) {
    private val subnetsIpv4 =
            acl.subnets.asIterable().filter { it.address is Inet4Address }.map { it to it.address.address }
    private val subnetsIpv6 =
            acl.subnets.asIterable().filter { it.address is Inet6Address }.map { it to it.address.address }
    private val bypassDomains = acl.bypassHostnames.asIterable().map { it.toRegex() }
    private val proxyDomains = acl.proxyHostnames.asIterable().map { it.toRegex() }
    private val bypass = acl.bypass

    fun shouldBypassIpv4(ip: ByteArray) = bypass xor subnetsIpv4.any { (subnet, subnetIp) -> subnet.matches(subnetIp, ip) }
    fun shouldBypassIpv6(ip: ByteArray) = bypass xor subnetsIpv6.any { (subnet, subnetIp) -> subnet.matches(subnetIp, ip) }
    fun shouldBypass(host: String): Boolean? {
        if (bypassDomains.any { it.matches(host) }) return true
        if (proxyDomains.any { it.matches(host) }) return false
        return null
    }
}
