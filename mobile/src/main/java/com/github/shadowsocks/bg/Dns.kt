/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import android.content.Context
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.os.Build
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.BuildConfig
import okhttp3.Dns as Okdns
import org.xbill.DNS.*
import java.net.Inet6Address
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.UnknownHostException
import java.util.*

object Dns {
    private const val TAG = "Dns"

    @Throws(Exception::class)
    fun getDnsResolver(context: Context): String {
        val dnsResolvers = getDnsResolvers(context)
        if (dnsResolvers.isEmpty()) throw Exception("Couldn't find an active DNS resolver")
        val dnsResolver = dnsResolvers.iterator().next().toString()
        if (dnsResolver.startsWith("/")) return dnsResolver.substring(1)
        return dnsResolver
    }

    @Throws(Exception::class)
    private fun getDnsResolvers(context: Context): Collection<InetAddress> {
        val addresses = ArrayList<InetAddress>()
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val classLinkProperties = Class.forName("android.net.LinkProperties")
        val getActiveLinkPropertiesMethod = ConnectivityManager::class.java.getMethod("getActiveLinkProperties")
        val linkProperties = getActiveLinkPropertiesMethod.invoke(connectivityManager)
        if (linkProperties != null) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                val dnses = classLinkProperties.getMethod("getDnses").invoke(linkProperties) as Collection<*>
                dnses.mapTo(addresses) { it as InetAddress }
            } else addresses.addAll((linkProperties as LinkProperties).dnsServers)
        }
        return addresses
    }

    private val hasIPv6Support get() = try {
        val result = NetworkInterface.getNetworkInterfaces().asSequence().flatMap { it.inetAddresses.asSequence() }
                .count { it is Inet6Address && !it.isLoopbackAddress && !it.isLinkLocalAddress } > 0
        if (result && BuildConfig.DEBUG) Log.d(TAG, "IPv6 address detected")
        result
    } catch (ex: Exception) {
        Log.e(TAG, "Failed to get interfaces' addresses.")
        app.track(ex)
        false
    }

    private fun resolve(host: String, addrType: Int): String? {
        try {
            val lookup = Lookup(host, addrType)
            val resolver = SimpleResolver("208.67.220.220")
            resolver.setTCP(true)
            resolver.setPort(443)
            resolver.setTimeout(5)
            lookup.setResolver(resolver)
            val records = (lookup.run() ?: return null).toMutableList()
            records.shuffle()
            for (r in records) {
                when (addrType) {
                    Type.A -> return (r as ARecord).address.hostAddress
                    Type.AAAA -> return (r as AAAARecord).address.hostAddress
                }
            }
        } catch (_: Exception) { }
        return null
    }
    private fun resolve(host: String): String? = try {
        InetAddress.getByName(host).hostAddress
    } catch (_: UnknownHostException) {
        null
    }
    fun resolve(host: String, enableIPv6: Boolean): String? =
            (if (enableIPv6 && hasIPv6Support) resolve(host, Type.AAAA) else null)
                    ?: resolve(host, Type.A)
                    ?: resolve(host)
}
