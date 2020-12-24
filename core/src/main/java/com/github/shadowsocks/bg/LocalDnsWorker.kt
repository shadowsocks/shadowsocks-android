package com.github.shadowsocks.bg

import com.github.shadowsocks.net.ConcurrentUdpSocketListener
import com.github.shadowsocks.net.DnsResolverCompat
import com.github.shadowsocks.utils.readableMessage
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.launch
import org.xbill.DNS.Message
import org.xbill.DNS.Rcode
import timber.log.Timber
import java.io.IOException
import java.net.SocketAddress
import java.nio.ByteBuffer
import java.nio.channels.DatagramChannel

class LocalDnsWorker(private val resolver: suspend (ByteArray) -> ByteArray, port: Int) : ConcurrentUdpSocketListener(
        "LocalDnsThread", port), CoroutineScope {

    override fun handle(channel: DatagramChannel, sender: SocketAddress, query: ByteBuffer) {
        launch {
            query.flip()
            val data = ByteArray(query.remaining())
            query.get(data)
            try {
                resolver(data)
            } catch (e: Exception) {
                when (e) {
                    is TimeoutCancellationException -> Timber.w("Resolving timed out")
                    is CancellationException -> {
                    } // ignore
                    is IOException -> Timber.d(e)
                    else -> Timber.w(e)
                }
                try {
                    DnsResolverCompat.prepareDnsResponse(Message(data)).apply {
                        header.rcode = Rcode.SERVFAIL
                    }.toWire()
                } catch (_: IOException) {
                    byteArrayOf()   // return empty if cannot parse packet
                }
            }?.let { r ->
                try {
                    val response = ByteBuffer.allocate(1024)
                    response.clear()
                    response.put(r)
                    response.flip()
                    channel.send(response, sender)
                } catch (e: IOException) {
                    Timber.d(e.readableMessage)
                }
            }
        }
    }
}
