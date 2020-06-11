package com.github.shadowsocks.bg

import android.net.LocalSocket
import com.github.shadowsocks.Core
import com.github.shadowsocks.net.ConcurrentLocalSocketListener
import com.github.shadowsocks.net.DnsResolverCompat
import com.github.shadowsocks.utils.readableMessage
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.launch
import org.xbill.DNS.Message
import org.xbill.DNS.Rcode
import timber.log.Timber
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.io.IOException

class LocalDnsWorker(private val resolver: suspend (ByteArray) -> ByteArray) : ConcurrentLocalSocketListener(
        "LocalDnsThread", File(Core.deviceStorage.noBackupFilesDir, "local_dns_path")), CoroutineScope {
    override fun acceptInternal(socket: LocalSocket) = error("big no no")
    override fun accept(socket: LocalSocket) {
        launch {
            socket.use {
                val input = DataInputStream(socket.inputStream)
                val query = try {
                    ByteArray(input.readUnsignedShort()).also { input.read(it) }
                } catch (e: IOException) {  // connection early close possibly due to resolving timeout
                    return@use Timber.d(e)
                }
                try {
                    resolver(query)
                } catch (e: Exception) {
                    when (e) {
                        is TimeoutCancellationException -> Timber.w("Resolving timed out")
                        is CancellationException -> { } // ignore
                        is IOException -> Timber.d(e)
                        else -> Timber.w(e)
                    }
                    try {
                        DnsResolverCompat.prepareDnsResponse(Message(query)).apply {
                            header.rcode = Rcode.SERVFAIL
                        }.toWire()
                    } catch (_: IOException) {
                        byteArrayOf()   // return empty if cannot parse packet
                    }
                }?.let { response ->
                    try {
                        val output = DataOutputStream(socket.outputStream)
                        output.writeShort(response.size)
                        output.write(response)
                    } catch (e: IOException) {
                        Timber.d(e.readableMessage)
                    }
                }
            }
        }
    }
}
