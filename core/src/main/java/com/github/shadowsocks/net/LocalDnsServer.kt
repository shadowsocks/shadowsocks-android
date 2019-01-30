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

import android.util.Log
import com.github.shadowsocks.utils.printLog
import kotlinx.coroutines.*
import org.xbill.DNS.*
import java.io.IOException
import java.net.*
import java.nio.ByteBuffer
import java.nio.channels.DatagramChannel
import java.nio.channels.SelectionKey
import java.nio.channels.SocketChannel

/**
 * A simple DNS conditional forwarder.
 *
 * No cache is provided as localResolver may change from time to time. We expect DNS clients to do cache themselves.
 *
 * Based on:
 *   https://github.com/bitcoinj/httpseed/blob/809dd7ad9280f4bc98a356c1ffb3d627bf6c7ec5/src/main/kotlin/dns.kt
 *   https://github.com/shadowsocks/overture/tree/874f22613c334a3b78e40155a55479b7b69fee04
 */
class LocalDnsServer(private val localResolver: suspend (String) -> Array<InetAddress>,
                     private val remoteDns: Socks5Endpoint, private val proxy: SocketAddress) : CoroutineScope {
    /**
     * Forward all requests to remote and ignore localResolver.
     */
    var forwardOnly = false
    /**
     * Forward UDP queries to TCP.
     */
    var tcp = true
    var remoteDomainMatcher: Regex? = null
    var localIpMatcher: List<Subnet> = emptyList()

    companion object {
        private const val TIMEOUT = 10_000L
        /**
         * TTL returned from localResolver is set to 120. Android API does not provide TTL,
         * so we suppose Android apps should not care about TTL either.
         */
        private const val TTL = 120L
        private const val UDP_PACKET_SIZE = 512
    }
    private val monitor = ChannelMonitor()

    private val job = SupervisorJob()
    override val coroutineContext = Dispatchers.Default + job + CoroutineExceptionHandler { _, t -> printLog(t) }

    fun start(listen: SocketAddress) = DatagramChannel.open().apply {
        configureBlocking(false)
        socket().bind(listen)
        monitor.register(this, SelectionKey.OP_READ) {
            val buffer = ByteBuffer.allocate(UDP_PACKET_SIZE)
            val source = receive(buffer)!!
            buffer.flip()
            launch {
                val reply = resolve(buffer)
                while (send(reply, source) <= 0) monitor.wait(this@apply, SelectionKey.OP_WRITE)
            }
        }
    }

    private suspend fun resolve(packet: ByteBuffer): ByteBuffer {
        val request = try {
            Message(packet)
        } catch (e: IOException) {  // we cannot parse the message, do not attempt to handle it at all
            printLog(e)
            return forward(packet)
        }
        try {
            if (forwardOnly || request.header.opcode != Opcode.QUERY) return forward(packet)
            val question = request.question
            if (question?.type != Type.A) return forward(packet)
            val host = question.name.toString(true)
            if (remoteDomainMatcher?.containsMatchIn(host) == true) return forward(packet)
            val localResults = try {
                withTimeout(TIMEOUT) { withContext(Dispatchers.IO) { localResolver(host) } }
            } catch (_: TimeoutCancellationException) {
                return forward(packet)
            } catch (_: UnknownHostException) {
                return forward(packet)
            }
            if (localResults.isEmpty()) return forward(packet)
            if (localIpMatcher.isEmpty() || localIpMatcher.any { subnet -> localResults.any(subnet::matches) }) {
                val response = Message(request.header.id)
                response.header.setFlag(Flags.QR.toInt())   // this is a response
                if (request.header.getFlag(Flags.RD.toInt())) response.header.setFlag(Flags.RD.toInt())
                response.header.setFlag(Flags.RA.toInt())   // recursion available
                response.addRecord(request.question, Section.QUESTION)
                for (address in localResults) response.addRecord(when (address) {
                    is Inet4Address -> ARecord(request.question.name, DClass.IN, TTL, address)
                    is Inet6Address -> AAAARecord(request.question.name, DClass.IN, TTL, address)
                    else -> throw IllegalStateException("Unsupported address $address")
                }, Section.ANSWER)
                return ByteBuffer.wrap(response.toWire())
            }
            return forward(packet)
        } catch (e: IOException) {
            printLog(e)
            val response = Message(request.header.id)
            response.header.rcode = Rcode.SERVFAIL
            response.header.setFlag(Flags.QR.toInt())
            if (request.header.getFlag(Flags.RD.toInt())) response.header.setFlag(Flags.RD.toInt())
            response.addRecord(request.question, Section.QUESTION)
            return ByteBuffer.wrap(response.toWire())
        }
    }

    private suspend fun forward(packet: ByteBuffer): ByteBuffer {
        packet.position(0)  // the packet might have been parsed, reset to beginning
        return withTimeout(TIMEOUT) {
            if (tcp) SocketChannel.open().use {
                it.configureBlocking(false)
                it.connect(proxy)
                val wrapped = remoteDns.tcpWrap(packet)
                while (!it.finishConnect()) monitor.wait(it, SelectionKey.OP_CONNECT)
                // monitor.waitWhile(it, SelectionKey.OP_WRITE) { it.write(wrapped) >= 0 && wrapped.hasRemaining() }
                while (it.write(wrapped) >= 0 && wrapped.hasRemaining()) monitor.wait(it, SelectionKey.OP_WRITE)
                remoteDns.tcpUnwrap(UDP_PACKET_SIZE, it::read) { monitor.wait(it, SelectionKey.OP_READ) }
            } else DatagramChannel.open().use {
                it.configureBlocking(false)
                monitor.wait(it, SelectionKey.OP_WRITE)
                check(it.send(remoteDns.udpWrap(packet), proxy) > 0)
                monitor.wait(it, SelectionKey.OP_READ)
                val result = remoteDns.udpReceiveBuffer(UDP_PACKET_SIZE)
                check(it.receive(result) == proxy)
                result.limit(result.position())
                remoteDns.udpUnwrap(result)
                result
            }.also { Log.d("forward", "completed $it") }
        }
    }

    suspend fun shutdown() {
        job.cancel()
        monitor.close()
        job.join()
    }
}
