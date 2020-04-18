package com.github.shadowsocks.bg

import android.net.LocalSocket
import android.util.Log
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.Core
import com.github.shadowsocks.net.ConcurrentLocalSocketListener
import com.github.shadowsocks.net.LocalDnsServer
import com.github.shadowsocks.utils.printLog
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.launch
import org.xbill.DNS.Message
import org.xbill.DNS.Rcode
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
                val query = ByteArray(input.readUnsignedShort())
                input.read(query)
                try {
                    resolver(query)
                } catch (e: Exception) {
                    when (e) {
                        is TimeoutCancellationException -> Crashlytics.log(Log.WARN, name, "Resolving timed out")
                        is CancellationException -> { } // ignore
                        is IOException -> Crashlytics.log(Log.WARN, name, e.message)
                        else -> printLog(e)
                    }
                    try {
                        LocalDnsServer.prepareDnsResponse(Message(query)).apply {
                            header.rcode = Rcode.SERVFAIL
                        }.toWire()
                    } catch (_: IOException) {
                        byteArrayOf()   // return empty if cannot parse packet
                    }
                }?.let { response ->
                    val output = DataOutputStream(socket.outputStream)
                    output.writeShort(response.size)
                    output.write(response)
                }
            }
        }
    }
}
