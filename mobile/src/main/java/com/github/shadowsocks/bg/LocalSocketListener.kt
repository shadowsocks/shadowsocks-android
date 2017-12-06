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
import com.github.shadowsocks.JniHelper
import java.io.File
import java.io.IOException
import java.util.concurrent.atomic.AtomicReference

abstract class LocalSocketListener(private val tag: String) : Thread() {
    init {
        setUncaughtExceptionHandler(app::track)
    }

    protected abstract val socketFile: File
    private val localSocket = AtomicReference<LocalSocket?>()

    /**
     * Inherited class do not need to close input/output streams as they will be closed automatically.
     */
    protected abstract fun accept(socket: LocalSocket)
    override fun run() {

        JniHelper.unlink(socketFile.absolutePath)
        Thread.sleep(1000) // trick to close the previous local socket safely

        val serverSocket: LocalServerSocket? = try {
            val ls = LocalSocket()
            localSocket.set(ls)
            ls.bind(LocalSocketAddress(socketFile.absolutePath, LocalSocketAddress.Namespace.FILESYSTEM))
            LocalServerSocket(ls.fileDescriptor)
        } catch (e: IOException) {
            Log.e(tag, "unable to bind", e)
            null
        }

        while (true) {
            try {
                val socket = serverSocket?.accept() ?: break

                try {
                    accept(socket)
                } catch (e: Exception) {
                    Log.e(tag, "Error when recv traffic stat", e)
                    app.track(e)
                }

                try {
                    socket.close()
                } catch (e: IOException) {
                    Log.e(tag, "Error when closing the local socket", e)
                    app.track(e)
                }

            } catch (e: IOException) {
                Log.e(tag, "Error when accept socket", e)
                app.track(e)
                break
            }
        }

        try {
            serverSocket?.close()
        } catch (e: IOException) {
            Log.e(tag, "Error when closing socket", e)
            app.track(e)
        }

        Log.d(tag, "thread exit")
    }

    fun stopThread() {
        val old = localSocket.getAndSet(null) ?: return
        try {
            old.close()
        } catch (_: Exception) { }  // ignore
    }
}
