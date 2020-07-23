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

import android.annotation.TargetApi
import android.net.DnsResolver
import android.net.Network
import android.os.Build
import android.os.CancellationSignal
import com.github.shadowsocks.Core
import kotlinx.coroutines.*
import org.xbill.DNS.*
import java.io.IOException
import java.net.Inet4Address
import java.net.Inet6Address
import java.net.InetAddress
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

sealed class DnsResolverCompat {
    companion object : DnsResolverCompat() {
        private val instance by lazy {
            when (Build.VERSION.SDK_INT) {
                in 29..Int.MAX_VALUE -> DnsResolverCompat29
                in 23 until 29 -> DnsResolverCompat23
                else -> error("Unsupported API level")
            }
        }

        override suspend fun resolve(network: Network, host: String) = instance.resolve(network, host)
        override suspend fun resolveOnActiveNetwork(host: String) = instance.resolveOnActiveNetwork(host)
        override suspend fun resolveRaw(network: Network, query: ByteArray) = instance.resolveRaw(network, query)
        override suspend fun resolveRawOnActiveNetwork(query: ByteArray) = instance.resolveRawOnActiveNetwork(query)

        // additional platform-independent DNS helpers

        /**
         * TTL returned from localResolver is set to 120. Android API does not provide TTL,
         * so we suppose Android apps should not care about TTL either.
         */
        private const val TTL = 120L

        fun prepareDnsResponse(request: Message) = Message(request.header.id).apply {
            header.setFlag(Flags.QR.toInt())    // this is a response
            header.setFlag(Flags.RA.toInt())    // recursion available
            if (request.header.getFlag(Flags.RD.toInt())) header.setFlag(Flags.RD.toInt())
            request.question?.also { addRecord(it, Section.QUESTION) }
        }
    }

    abstract suspend fun resolve(network: Network, host: String): Array<InetAddress>
    abstract suspend fun resolveOnActiveNetwork(host: String): Array<InetAddress>
    abstract suspend fun resolveRaw(network: Network, query: ByteArray): ByteArray
    abstract suspend fun resolveRawOnActiveNetwork(query: ByteArray): ByteArray

    private object DnsResolverCompat23 : DnsResolverCompat() {
        /**
         * This dispatcher is used for noncancellable possibly-forever-blocking operations in network IO.
         *
         * See also: https://issuetracker.google.com/issues/133874590
         */
        private val unboundedIO by lazy {
            if (Core.activity.isLowRamDevice) Dispatchers.IO
            else Executors.newCachedThreadPool().asCoroutineDispatcher()
        }

        override suspend fun resolve(network: Network, host: String) =
                withContext(unboundedIO) { network.getAllByName(host) }
        override suspend fun resolveOnActiveNetwork(host: String) =
                withContext(unboundedIO) { InetAddress.getAllByName(host) }

        private suspend fun resolveRaw(query: ByteArray, networkSpecified: Boolean = true,
                                       hostResolver: suspend (String) -> Array<InetAddress>): ByteArray {
            val request = try {
                Message(query)
            } catch (e: IOException) {
                throw UnsupportedOperationException(e)  // unrecognized packet
            }
            when (val opcode = request.header.opcode) {
                Opcode.QUERY -> { }
                else -> throw UnsupportedOperationException("Unsupported opcode $opcode")
            }
            val question = request.question
            val isIpv6 = when (val type = question?.type) {
                Type.A -> false
                Type.AAAA -> true
                Type.PTR -> {
                    /* Android does not provide a PTR lookup API for Network prior to Android 10 */
                    if (networkSpecified) throw IOException(UnsupportedOperationException("Network unspecified"))
                    val ip = try {
                        ReverseMap.fromName(question.name)
                    } catch (e: IOException) {
                        throw UnsupportedOperationException(e)  // unrecognized PTR name
                    }
                    val hostname = withContext(unboundedIO) { ip.hostName }.let { hostname ->
                        if (hostname == ip.hostAddress) null else Name.fromString("$hostname.")
                    }
                    return prepareDnsResponse(request).apply {
                        hostname?.let { addRecord(PTRRecord(question.name, DClass.IN, TTL, it), Section.ANSWER) }
                    }.toWire()
                }
                else -> throw UnsupportedOperationException("Unsupported query type $type")
            }
            val host = question.name.canonicalize().toString(true)
            return prepareDnsResponse(request).apply {
                for (address in hostResolver(host).asIterable().run {
                    if (isIpv6) filterIsInstance<Inet6Address>() else filterIsInstance<Inet4Address>()
                }) addRecord(when (address) {
                    is Inet4Address -> ARecord(question.name, DClass.IN, TTL, address)
                    is Inet6Address -> AAAARecord(question.name, DClass.IN, TTL, address)
                    else -> error("Unsupported address $address")
                }, Section.ANSWER)
            }.toWire()
        }
        override suspend fun resolveRaw(network: Network, query: ByteArray) =
                resolveRaw(query) { resolve(network, it) }
        override suspend fun resolveRawOnActiveNetwork(query: ByteArray) =
                resolveRaw(query, false, this::resolveOnActiveNetwork)
    }

    @TargetApi(29)
    private object DnsResolverCompat29 : DnsResolverCompat(), Executor {
        /**
         * This executor will run on its caller directly. On Q beta 3 thru 4, this results in calling in main thread.
         */
        override fun execute(command: Runnable) = command.run()

        private val activeNetwork get() = Core.connectivity.activeNetwork ?: throw IOException("no network")

        override suspend fun resolve(network: Network, host: String): Array<InetAddress> {
            return suspendCancellableCoroutine { cont ->
                val signal = CancellationSignal()
                cont.invokeOnCancellation { signal.cancel() }
                // retry should be handled by client instead
                DnsResolver.getInstance().query(network, host, DnsResolver.FLAG_NO_RETRY, this,
                        signal, object : DnsResolver.Callback<Collection<InetAddress>> {
                    override fun onAnswer(answer: Collection<InetAddress>, rcode: Int) =
                            cont.resume(answer.toTypedArray())
                    override fun onError(error: DnsResolver.DnsException) = cont.resumeWithException(IOException(error))
                })
            }
        }
        override suspend fun resolveOnActiveNetwork(host: String) = resolve(activeNetwork, host)

        override suspend fun resolveRaw(network: Network, query: ByteArray): ByteArray {
            return suspendCancellableCoroutine { cont ->
                val signal = CancellationSignal()
                cont.invokeOnCancellation { signal.cancel() }
                DnsResolver.getInstance().rawQuery(network, query, DnsResolver.FLAG_NO_RETRY, this,
                        signal, object : DnsResolver.Callback<ByteArray> {
                    override fun onAnswer(answer: ByteArray, rcode: Int) = cont.resume(answer)
                    override fun onError(error: DnsResolver.DnsException) = cont.resumeWithException(IOException(error))
                })
            }
        }
        override suspend fun resolveRawOnActiveNetwork(query: ByteArray) = resolveRaw(activeNetwork, query)
    }
}
