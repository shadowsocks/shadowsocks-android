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

import android.annotation.SuppressLint
import android.annotation.TargetApi
import android.net.DnsResolver
import android.net.Network
import android.os.Build
import android.os.CancellationSignal
import android.os.Looper
import android.system.ErrnoException
import android.system.Os
import com.github.shadowsocks.Core
import com.github.shadowsocks.utils.int
import kotlinx.coroutines.*
import org.xbill.DNS.*
import java.io.FileDescriptor
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
                in 21 until 23 -> DnsResolverCompat21()
                else -> error("Unsupported API level")
            }
        }

        override fun bindSocket(network: Network, socket: FileDescriptor) = instance.bindSocket(network, socket)
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
            if (request.header.getFlag(Flags.RD.toInt())) header.setFlag(Flags.RD.toInt())
            request.question?.also { addRecord(it, Section.QUESTION) }
        }
    }

    @Throws(IOException::class)
    abstract fun bindSocket(network: Network, socket: FileDescriptor)
    internal open suspend fun connectUdp(fd: FileDescriptor, address: InetAddress, port: Int = 0) =
            Os.connect(fd, address, port)
    abstract suspend fun resolve(network: Network, host: String): Array<InetAddress>
    abstract suspend fun resolveOnActiveNetwork(host: String): Array<InetAddress>
    abstract suspend fun resolveRaw(network: Network, query: ByteArray): ByteArray
    abstract suspend fun resolveRawOnActiveNetwork(query: ByteArray): ByteArray

    @SuppressLint("PrivateApi")
    private open class DnsResolverCompat21 : DnsResolverCompat() {
        private val bindSocketToNetwork by lazy {
            Class.forName("android.net.NetworkUtils").getDeclaredMethod(
                    "bindSocketToNetwork", Int::class.java, Int::class.java)
        }
        private val netId by lazy { Network::class.java.getDeclaredField("netId") }
        override fun bindSocket(network: Network, socket: FileDescriptor) {
            val netId = netId.get(network)!!
            val err = bindSocketToNetwork.invoke(null, socket.int, netId) as Int
            if (err == 0) return
            val message = "Binding socket to network $netId"
            throw IOException(message, ErrnoException(message, -err))
        }

        override suspend fun connectUdp(fd: FileDescriptor, address: InetAddress, port: Int) {
            if (Looper.getMainLooper().thread == Thread.currentThread()) withContext(Dispatchers.IO) {  // #2405
                super.connectUdp(fd, address, port)
            } else super.connectUdp(fd, address, port)
        }

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
                GlobalScope.async(unboundedIO) { network.getAllByName(host) }.await()
        override suspend fun resolveOnActiveNetwork(host: String) =
                GlobalScope.async(unboundedIO) { InetAddress.getAllByName(host) }.await()

        private suspend fun resolveRaw(query: ByteArray,
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
                else -> throw UnsupportedOperationException("Unsupported query type $type")
            }
            val host = question.name.canonicalize().toString(true)
            return prepareDnsResponse(request).apply {
                header.setFlag(Flags.RA.toInt())   // recursion available
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
                resolveRaw(query, this::resolveOnActiveNetwork)
    }

    @TargetApi(23)
    private object DnsResolverCompat23 : DnsResolverCompat21() {
        override fun bindSocket(network: Network, socket: FileDescriptor) = network.bindSocket(socket)
    }

    @TargetApi(29)
    private object DnsResolverCompat29 : DnsResolverCompat(), Executor {
        /**
         * This executor will run on its caller directly. On Q beta 3 thru 4, this results in calling in main thread.
         */
        override fun execute(command: Runnable) = command.run()

        override fun bindSocket(network: Network, socket: FileDescriptor) = network.bindSocket(socket)

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
