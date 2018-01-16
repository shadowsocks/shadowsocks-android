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

import android.net.LocalSocket
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

class TrafficMonitorThread : LocalSocketListener("TrafficMonitorThread") {
    override val socketFile = File(app.deviceContext.filesDir, "stat_path")

    override fun accept(socket: LocalSocket) {
        try {
            val buffer = ByteArray(16)
            if (socket.inputStream.read(buffer) != 16) throw IOException("Unexpected traffic stat length")
            val stat = ByteBuffer.wrap(buffer).order(ByteOrder.LITTLE_ENDIAN)
            TrafficMonitor.update(stat.getLong(0), stat.getLong(8))
            socket.outputStream.write(0)
        } catch (e: Exception) {
            Log.e(tag, "Error when recv traffic stat", e)
            app.track(e)
        }
    }
}
