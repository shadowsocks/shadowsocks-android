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

package com.github.shadowsocks.utils

import java.net.InetAddress
import java.util.*

class Subnet(val address: InetAddress, val prefixSize: Int) : Comparable<Subnet> {
    companion object {
        fun fromString(value: String): Subnet? {
            @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN")
            val parts = (value as java.lang.String).split("/", 2)
            val addr = parts[0].parseNumericAddress() ?: return null
            return if (parts.size == 2) try {
                val prefixSize = parts[1].toInt()
                if (prefixSize < 0 || prefixSize > addr.address.size shl 3) null else Subnet(addr, prefixSize)
            } catch (_: NumberFormatException) {
                null
            } else Subnet(addr, addr.address.size shl 3)
        }
    }

    private val addressLength get() = address.address.size shl 3

    init {
        if (prefixSize < 0 || prefixSize > addressLength) throw IllegalArgumentException()
    }

    override fun toString(): String =
            if (prefixSize == addressLength) address.hostAddress else address.hostAddress + '/' + prefixSize

    private fun Byte.unsigned() = toInt() and 0xFF
    override fun compareTo(other: Subnet): Int {
        val addrThis = address.address
        val addrThat = other.address.address
        var result = addrThis.size.compareTo(addrThat.size) // IPv4 address goes first
        if (result != 0) return result
        for ((x, y) in addrThis zip addrThat) {
            result = x.unsigned().compareTo(y.unsigned())   // undo sign extension of signed byte
            if (result != 0) return result
        }
        return prefixSize.compareTo(other.prefixSize)
    }

    override fun equals(other: Any?): Boolean {
        val that = other as? Subnet
        return address == that?.address && prefixSize == that.prefixSize
    }
    override fun hashCode(): Int = Objects.hash(address, prefixSize)
}
