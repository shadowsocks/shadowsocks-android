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

import android.os.SystemClock
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.R
import java.text.DecimalFormat

object TrafficMonitor {
    // Bytes per second
    var txRate = 0L
    var rxRate = 0L

    // Bytes for the current session
    var txTotal = 0L
    var rxTotal = 0L

    // Bytes for the last query
    private var txLast = 0L
    private var rxLast = 0L
    private var timestampLast = 0L
    @Volatile
    private var dirty = true

    private val units = arrayOf("KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "BB", "NB", "DB", "CB")
    private val numberFormat = DecimalFormat("@@@")
    fun formatTraffic(size: Long): String {
        var n: Double = size.toDouble()
        var i = -1
        while (n >= 999.5) {
            n /= 1024
            ++i
        }
        return if (i < 0) "$size ${app.resources.getQuantityString(R.plurals.bytes, size.toInt())}"
        else "${numberFormat.format(n)} ${units[i]}"
    }

    fun updateRate(): Boolean {
        val now = SystemClock.elapsedRealtime()
        val delta = now - timestampLast
        timestampLast = now
        var updated = false
        if (delta != 0L)
            if (dirty) {
                txRate = (txTotal - txLast) * 1000 / delta
                rxRate = (rxTotal - rxLast) * 1000 / delta
                txLast = txTotal
                rxLast = rxTotal
                dirty = false
                updated = true
            } else {
                if (txRate != 0L) {
                    txRate = 0
                    updated = true
                }
                if (rxRate != 0L) {
                    rxRate = 0
                    updated = true
                }
            }
        return updated
    }

    fun update(tx: Long, rx: Long) {
        if (txTotal != tx) {
            txTotal = tx
            dirty = true
        }
        if (rxTotal != rx) {
            rxTotal = rx
            dirty = true
        }
    }

    fun reset() {
        txRate = 0
        rxRate = 0
        txTotal = 0
        rxTotal = 0
        txLast = 0
        rxLast = 0
        dirty = true
    }
}
