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

package com.github.shadowsocks.net

import android.annotation.SuppressLint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.sendBlocking
import kotlinx.coroutines.launch
import timber.log.Timber
import java.io.IOException
import java.net.InetSocketAddress
import java.net.SocketAddress
import java.nio.ByteBuffer
import java.nio.channels.DatagramChannel

abstract class UdpSocketListener(name: String, val port: Int) : Thread(name) {

    private val udpChannel = DatagramChannel.open()
    private val closeChannel = Channel<Unit>(1)

    @Volatile
    protected var running = true

    /**
     * Inherited class do not need to close input/output streams as they will be closed automatically.
     */
    protected abstract fun handle(channel: DatagramChannel, sender: SocketAddress, query: ByteBuffer)

    final override fun run() {
        udpChannel.socket().bind(InetSocketAddress(port))
        udpChannel.configureBlocking(true)
        udpChannel.use {
            while (running) {
                try {
                    val query = ByteBuffer.allocate(65536)
                    query.clear()
                    udpChannel.receive(query)?.let { handle(udpChannel, it, query) }
                } catch (e: IOException) {
                    if (running) Timber.w(e)
                    continue
                }
            }
        }
        closeChannel.sendBlocking(Unit)
    }

    @SuppressLint("NewApi")
    open fun shutdown(scope: CoroutineScope) {
        running = false
        udpChannel.close()
        scope.launch { closeChannel.receive() }
    }
}
