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

package com.github.shadowsocks.net

import net.sourceforge.jsocks.Socks5Message
import java.io.ByteArrayInputStream
import java.net.*
import java.nio.ByteBuffer

class Socks5DatagramSocket(proxy: Proxy) : DatagramSocket() {
    private val proxy = proxy.address() as InetSocketAddress

    override fun send(dp: DatagramPacket) {
        val data = ByteBuffer.allocate(6 + dp.address.address.size + dp.length).apply {
            // header
            put(Socks5Message.SOCKS_VERSION.toByte())
            putShort(0)
            put(when (dp.address) {
                is Inet4Address -> Socks5Message.SOCKS_ATYP_IPV4
                is Inet6Address -> Socks5Message.SOCKS_ATYP_IPV6
                else -> throw IllegalStateException("Unsupported address type")
            }.toByte())
            put(dp.address.address)
            putShort(dp.port.toShort())
            // data
            put(dp.data, dp.offset, dp.length)
        }.array()
        super.send(DatagramPacket(data, data.size, proxy.address, proxy.port))
    }

    override fun receive(dp: DatagramPacket) {
        super.receive(dp)
        check(proxy.address == dp.address && proxy.port == dp.port) { "Unexpected packet" }
        val stream = ByteArrayInputStream(dp.data, dp.offset, dp.length)
        val msg = Socks5Message(stream)
        dp.port = msg.port
        dp.address = msg.inetAddress
        val remaining = stream.available()
        dp.setData(dp.data, dp.offset + dp.length - remaining, remaining)
    }
}
