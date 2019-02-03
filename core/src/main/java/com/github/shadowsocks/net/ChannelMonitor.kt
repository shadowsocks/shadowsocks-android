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

import com.github.shadowsocks.utils.printLog
import kotlinx.coroutines.suspendCancellableCoroutine
import java.io.IOException
import java.lang.IllegalStateException
import java.nio.ByteBuffer
import java.nio.channels.*
import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.coroutines.resume

class ChannelMonitor : Thread("ChannelMonitor"), AutoCloseable {
    private val selector = Selector.open()
    private val registrationPipe = Pipe.open()
    private val pendingRegistrations = ConcurrentLinkedQueue<Triple<SelectableChannel, Int, (SelectionKey) -> Unit>>()
    @Volatile
    private var running = true

    private fun registerInternal(channel: SelectableChannel, ops: Int, block: (SelectionKey) -> Unit) =
            channel.register(selector, ops, block)

    init {
        registrationPipe.source().apply {
            configureBlocking(false)
            registerInternal(this, SelectionKey.OP_READ) {
                val junk = ByteBuffer.allocateDirect(1)
                while (read(junk) > 0) {
                    val (channel, ops, block) = pendingRegistrations.remove()
                    registerInternal(channel, ops, block)
                    junk.clear()
                }
            }
        }
        start()
    }

    fun register(channel: SelectableChannel, ops: Int, block: (SelectionKey) -> Unit) {
        pendingRegistrations.add(Triple(channel, ops, block))
        val junk = ByteBuffer.allocateDirect(1)
        while (running && registrationPipe.sink().write(junk) == 0);
    }

    suspend fun wait(channel: SelectableChannel, ops: Int) = suspendCancellableCoroutine<Unit> { cont ->
        register(channel, ops) {
            if (it.isValid) it.interestOps(0)       // stop listening
            try {
                cont.resume(Unit)
            } catch (_: IllegalStateException) { }  // already resumed by a timeout, maybe should use tryResume?
        }
    }

    override fun run() {
        while (running) {
            val num = try {
                selector.select()
            } catch (e: IOException) {
                printLog(e)
                continue
            }
            if (num <= 0) continue
            val iterator = selector.selectedKeys().iterator()
            while (iterator.hasNext()) {
                val key = iterator.next()
                iterator.remove()
                (key.attachment() as (SelectionKey) -> Unit)(key)
            }
        }
    }

    override fun close() {
        running = false
        selector.wakeup()
        join()
        selector.keys().forEach { it.channel().close() }
        selector.close()
    }
}
