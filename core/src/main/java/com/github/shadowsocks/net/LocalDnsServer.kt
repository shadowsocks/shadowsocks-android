package com.github.shadowsocks.net

import android.os.ParcelFileDescriptor
import android.util.Log
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.parseNumericAddress
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.shutdown
import kotlinx.coroutines.*
import net.sourceforge.jsocks.Socks5DatagramSocket
import net.sourceforge.jsocks.Socks5Proxy
import org.xbill.DNS.*
import java.io.Closeable
import java.io.FileDescriptor
import java.io.IOException
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
        private const val UDP_PACKET_SIZE = 1500

    }
    private val socket = DatagramSocket(DataStore.portLocalDns, DataStore.listenAddress.parseNumericAddress())
    private val DatagramSocket.fileDescriptor get() = ParcelFileDescriptor.fromDatagramSocket(this).fileDescriptor
    override val fileDescriptor get() = socket.fileDescriptor
    private val tcpProxy = DataStore.proxy
    private val udpProxy = Socks5Proxy("127.0.0.1", DataStore.portProxy)

    private val activeFds = Collections.newSetFromMap(ConcurrentHashMap<FileDescriptor, Boolean>())
    private val job = SupervisorJob()
    override val coroutineContext get() = Dispatchers.Default + job + CoroutineExceptionHandler { _, t -> printLog(t) }

    override fun run() {
        while (running) {
            val packet = DatagramPacket(ByteArray(UDP_PACKET_SIZE), 0, UDP_PACKET_SIZE)
            try {
                socket.receive(packet)
                launch(start = CoroutineStart.UNDISPATCHED) {
                    resolve(packet) // this method should also put the reply in the packet
                    socket.send(packet)
                }
            } catch (e: RuntimeException) {
                e.printStackTrace()
            }
        }
    }

    private suspend fun <T> io(block: suspend CoroutineScope.() -> T) =
            withTimeout(TIMEOUT) { withContext(Dispatchers.IO, block) }

    private suspend fun resolve(packet: DatagramPacket) {
        if (forwardOnly) return forward(packet)
        val request = try {
            Message(ByteBuffer.wrap(packet.data, packet.offset, packet.length))
        } catch (e: IOException) {
            printLog(e)
            return forward(packet)
        }
        if (request.header.opcode != Opcode.QUERY || request.header.rcode != Rcode.NOERROR) return forward(packet)
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
            Log.d("DNS", "$host (local) -> $localResults")
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
    }

    private suspend fun forward(packet: DatagramPacket) = if (tcp) Socket(tcpProxy).useFd {
        it.connect(InetSocketAddress(remoteDns, 53), 53)
        it.getOutputStream().apply {
            write(packet.data, packet.offset, packet.length)
            flush()
        }
        val read = it.getInputStream().read(packet.data, 0, UDP_PACKET_SIZE)
        packet.length = if (read < 0) 0 else read
    } else Socks5DatagramSocket(udpProxy, 0, null).useFd {
        val address = packet.address    // we are reusing the packet, save it first
        packet.address = remoteDns
        packet.port = 53
        packet.toString()
        Log.d("DNS", "Sending $packet")
        it.send(packet)
        Log.d("DNS", "Receiving $packet")
        it.receive(packet)
        Log.d("DNS", "Finished $packet")
        packet.address = address
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
