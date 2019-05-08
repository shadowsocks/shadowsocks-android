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
import android.net.DnsResolver
import android.net.Network
import android.net.ParseException
import android.os.CancellationSignal
import android.system.ErrnoException
import androidx.core.os.BuildCompat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.suspendCancellableCoroutine
import java.net.InetAddress
import java.util.concurrent.Executor
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

sealed class DnsResolverCompat {
    companion object : DnsResolverCompat() {
        private val instance by lazy { if (BuildCompat.isAtLeastQ()) DnsResolverCompat29 else DnsResolverCompat21 }

        override suspend fun resolve(network: Network, host: String) = instance.resolve(network, host)
    }

    abstract suspend fun resolve(network: Network, host: String): Array<InetAddress>

    private object DnsResolverCompat21 : DnsResolverCompat() {
        override suspend fun resolve(network: Network, host: String) =
                GlobalScope.async(Dispatchers.IO) { network.getAllByName(host) }.await()
    }

    @TargetApi(29)
    private object DnsResolverCompat29 : DnsResolverCompat() {
        /**
         * This executor will run on its caller directly. On Q beta 3, this is called in main thread.
         */
        private val executor = Executor { it.run() }
        override suspend fun resolve(network: Network, host: String): Array<InetAddress> {
            return suspendCancellableCoroutine { cont ->
                val signal = CancellationSignal()
                cont.invokeOnCancellation { signal.cancel() }
                // retry should be handled by client instead
                DnsResolver.getInstance().query(network, host, DnsResolver.FLAG_NO_RETRY, executor,
                        signal, object : DnsResolver.InetAddressAnswerCallback() {
                    override fun onAnswer(answer: MutableList<InetAddress>) = cont.resume(answer.toTypedArray())
                    override fun onQueryException(exception: ErrnoException) = cont.resumeWithException(exception)
                    override fun onParseException(exception: ParseException) = cont.resumeWithException(exception)
                })
            }
        }
    }
}
