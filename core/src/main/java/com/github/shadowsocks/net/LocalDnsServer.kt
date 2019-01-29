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

import android.os.ParcelFileDescriptor
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.parseNumericAddress
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.shutdown
import kotlinx.coroutines.*
import org.xbill.DNS.*
import java.io.*
import java.net.*
import java.nio.ByteBuffer
import java.util.*
import java.util.concurrent.ConcurrentHashMap

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
                     private val remoteDns: InetAddress) : SocketListener("LocalDnsServer"), CoroutineScope {
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
    private val socket = DatagramSocket(DataStore.portLocalDns, DataStore.listenAddress.parseNumericAddress())
    private val DatagramSocket.fileDescriptor get() = ParcelFileDescriptor.fromDatagramSocket(this).fileDescriptor
    override val fileDescriptor get() = socket.fileDescriptor
    private val proxy = DataStore.proxy

    private val activeFds = Collections.newSetFromMap(ConcurrentHashMap<FileDescriptor, Boolean>())
    private val job = SupervisorJob()
    override val coroutineContext get() = Dispatchers.Default + job + CoroutineExceptionHandler { _, t -> printLog(t) }

    override fun run() {
        while (running) {
            val packet = DatagramPacket(ByteArray(UDP_PACKET_SIZE), 0, UDP_PACKET_SIZE)
            try {
                socket.receive(packet)
                launch {
                    resolve(packet) // this method should also put the reply in the packet
                    socket.send(packet)
                }
            } catch (e: RuntimeException) {
                e.printStackTrace()
            }
        }
        socket.close()
    }

    private suspend fun <T> io(block: suspend CoroutineScope.() -> T) =
            withTimeout(TIMEOUT) { withContext(Dispatchers.IO, block) }

    private suspend fun resolve(packet: DatagramPacket) {
        val request = try {
            Message(ByteBuffer.wrap(packet.data, packet.offset, packet.length))
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
                io { localResolver(host) }
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
                val wire = response.toWire()
                return packet.setData(wire, 0, wire.size)
            }
            return forward(packet)
        } catch (e: IOException) {
            printLog(e)
            val response = Message(request.header.id)
            response.header.rcode = Rcode.SERVFAIL
            response.header.setFlag(Flags.QR.toInt())
            if (request.header.getFlag(Flags.RD.toInt())) response.header.setFlag(Flags.RD.toInt())
            response.addRecord(request.question, Section.QUESTION)
            val wire = response.toWire()
            return packet.setData(wire, 0, wire.size)
        }
    }

    private suspend fun forward(packet: DatagramPacket) = if (tcp) Socket(proxy).useFd {
        it.connect(InetSocketAddress(remoteDns, 53))
        DataOutputStream(it.getOutputStream()).apply {
            writeShort(packet.length)
            write(packet.data, packet.offset, packet.length)
            flush()
        }
        DataInputStream(it.getInputStream()).apply {
            packet.length = readUnsignedShort()
            readFully(packet.data, packet.offset, packet.length)
        }
    } else Socks5DatagramSocket(proxy).useFd {
        val address = packet.address    // we are reusing the packet, save it first
        val port = packet.port
        packet.address = remoteDns
        packet.port = 53
        it.send(packet)
        packet.length = UDP_PACKET_SIZE
        it.receive(packet)
        packet.address = address
        packet.port = port
    }

    private suspend fun <T : Closeable> T.useFd(block: (T) -> Unit) {
        val fd = when (this) {
            is Socket -> ParcelFileDescriptor.fromSocket(this).fileDescriptor
            is DatagramSocket -> fileDescriptor
            else -> throw IllegalStateException("Unsupported type $javaClass for obtaining FileDescriptor")
        }
        try {
            activeFds += fd
            io { use(block) }
        } finally {
            fd.shutdown()
            activeFds -= fd
        }
    }

    suspend fun shutdown() {
        running = false
        job.cancel()
        close()
        activeFds.forEach { it.shutdown() }
        job.join()
    }
}
