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

package com.github.shadowsocks.bg

import android.annotation.TargetApi
import android.app.ActivityManager
import android.net.DnsResolver
import android.net.Network
import android.os.Build
import android.os.CancellationSignal
import androidx.core.content.getSystemService
import com.github.shadowsocks.Core
import com.github.shadowsocks.Core.app
import kotlinx.coroutines.*
import java.io.IOException
import java.lang.Runnable
import java.net.InetAddress
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

sealed class DnsResolverCompat {
    companion object : DnsResolverCompat() {
        private val instance by lazy { if (Build.VERSION.SDK_INT >= 29) DnsResolverCompat29 else DnsResolverCompat21 }

        override suspend fun resolve(network: Network, host: String) = instance.resolve(network, host)
        override suspend fun resolveOnActiveNetwork(host: String) = instance.resolveOnActiveNetwork(host)
    }

    abstract suspend fun resolve(network: Network, host: String): Array<InetAddress>
    abstract suspend fun resolveOnActiveNetwork(host: String): Array<InetAddress>

    private object DnsResolverCompat21 : DnsResolverCompat() {
        /**
         * This dispatcher is used for noncancellable possibly-forever-blocking operations in network IO.
         *
         * See also: https://issuetracker.google.com/issues/133874590
         */
        private val unboundedIO by lazy {
            if (app.getSystemService<ActivityManager>()!!.isLowRamDevice) Dispatchers.IO
            else Executors.newCachedThreadPool().asCoroutineDispatcher()
        }

        override suspend fun resolve(network: Network, host: String) =
                GlobalScope.async(unboundedIO) { network.getAllByName(host) }.await()
        override suspend fun resolveOnActiveNetwork(host: String) =
                GlobalScope.async(unboundedIO) { InetAddress.getAllByName(host) }.await()
    }

    @TargetApi(29)
    private object DnsResolverCompat29 : DnsResolverCompat(), Executor {
        /**
         * This executor will run on its caller directly. On Q beta 3 thru 4, this results in calling in main thread.
         */
        override fun execute(command: Runnable) = command.run()

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

        override suspend fun resolveOnActiveNetwork(host: String): Array<InetAddress> {
            return resolve(Core.connectivity.activeNetwork ?: return emptyArray(), host)
        }
    }
}
