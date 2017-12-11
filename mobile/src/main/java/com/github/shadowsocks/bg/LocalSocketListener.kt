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

import android.net.LocalServerSocket
import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import java.io.File
import java.io.IOException

abstract class LocalSocketListener(protected val tag: String) : Thread() {
    init {
        setUncaughtExceptionHandler(app::track)
    }

    protected abstract val socketFile: File
    @Volatile
    private var running = true

    /**
     * Inherited class do not need to close input/output streams as they will be closed automatically.
     */
    protected abstract fun accept(socket: LocalSocket)
    override final fun run() {
        socketFile.delete() // It's a must-have to close and reuse previous local socket.
        LocalSocket().use { localSocket ->
            val serverSocket = try {
                localSocket.bind(LocalSocketAddress(socketFile.absolutePath, LocalSocketAddress.Namespace.FILESYSTEM))
                LocalServerSocket(localSocket.fileDescriptor)
            } catch (e: IOException) {
                Log.e(tag, "unable to bind", e)
                return
            }
            while (running) {
                try {
                    serverSocket.accept()
                } catch (e: IOException) {
                    Log.e(tag, "Error when accept socket", e)
                    app.track(e)
                    null
                }?.use(this::accept)
            }
        }
    }

    fun stopThread() {
        running = false
        interrupt()
    }
}
