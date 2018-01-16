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

package com.github.shadowsocks.database

import android.net.Uri
import android.util.Base64
import android.util.Log
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.parsePort
import com.j256.ormlite.field.DataType
import com.j256.ormlite.field.DatabaseField
import java.io.Serializable
import java.net.URI
import java.util.*

class Profile : Serializable {
    companion object {
        private const val TAG = "ShadowParser"
        private val pattern = """(?i)ss://[-a-zA-Z0-9+&@#/%?=~_|!:,.;\[\]]*[-a-zA-Z0-9+&@#/%=~_|\[\]]""".toRegex()
        private val userInfoPattern = "^(.+?):(.*)$".toRegex()
        private val legacyPattern = "^(.+?):(.*)@(.+?):(\\d+?)$".toRegex()

        fun findAll(data: CharSequence?) = pattern.findAll(data ?: "").map {
            val uri = Uri.parse(it.value)
            if (uri.userInfo == null) {
                val match = legacyPattern.matchEntire(String(Base64.decode(uri.host, Base64.NO_PADDING)))
                if (match != null) {
                    val profile = Profile()
                    profile.method = match.groupValues[1].toLowerCase()
                    profile.password = match.groupValues[2]
                    profile.host = match.groupValues[3]
                    profile.remotePort = match.groupValues[4].toInt()
                    profile.plugin = uri.getQueryParameter(Key.plugin)
                    profile.name = uri.fragment
                    profile
                } else {
                    Log.e(TAG, "Unrecognized URI: ${it.value}")
                    null
                }
            } else {
                val match = userInfoPattern.matchEntire(String(Base64.decode(uri.userInfo,
                        Base64.NO_PADDING or Base64.NO_WRAP or Base64.URL_SAFE)))
                if (match != null) {
                    val profile = Profile()
                    profile.method = match.groupValues[1]
                    profile.password = match.groupValues[2]
                    // bug in Android: https://code.google.com/p/android/issues/detail?id=192855
                    val javaURI = URI(it.value)
                    profile.host = javaURI.host
                    if (profile.host.firstOrNull() == '[' && profile.host.lastOrNull() == ']')
                        profile.host = profile.host.substring(1, profile.host.length - 1)
                    profile.remotePort = javaURI.port
                    profile.plugin = uri.getQueryParameter(Key.plugin)
                    profile.name = uri.fragment ?: ""
                    profile
                } else {
                    Log.e(TAG, "Unknown user info: ${it.value}")
                    null
                }
            }
        }.filterNotNull()
    }

    @DatabaseField(generatedId = true)
    var id: Int = 0

    @DatabaseField
    var name: String? = ""

    @DatabaseField
    var host: String = "198.199.101.152"

    @DatabaseField
    var remotePort: Int = 8388

    @DatabaseField
    var password: String = "u1rRWTssNv0p"

    @DatabaseField
    var method: String = "aes-256-cfb"

    @DatabaseField
    var route: String = "all"

    @DatabaseField
    var remoteDns: String = "8.8.8.8"

    @DatabaseField
    var proxyApps: Boolean = false

    @DatabaseField
    var bypass: Boolean = false

    @DatabaseField
    var udpdns: Boolean = false

    @DatabaseField
    var ipv6: Boolean = true

    @DatabaseField(dataType = DataType.LONG_STRING)
    var individual: String = ""

    @DatabaseField
    var tx: Long = 0

    @DatabaseField
    var rx: Long = 0

    @DatabaseField
    val date: Date = Date()

    @DatabaseField
    var userOrder: Long = 0

    @DatabaseField
    var plugin: String? = null

    val formattedAddress get() = (if (host.contains(":")) "[%s]:%d" else "%s:%d").format(host, remotePort)
    val formattedName get() = if (name.isNullOrEmpty()) formattedAddress else name!!

    fun toUri(): Uri {
        val builder = Uri.Builder()
                .scheme("ss")
                .encodedAuthority("%s@%s:%d".format(Locale.ENGLISH,
                        Base64.encodeToString("%s:%s".format(Locale.ENGLISH, method, password).toByteArray(),
                                Base64.NO_PADDING or Base64.NO_WRAP or Base64.URL_SAFE),
                        if (host.contains(':')) "[$host]" else host, remotePort))
        val configuration = PluginConfiguration(plugin ?: "")
        if (configuration.selected.isNotEmpty())
            builder.appendQueryParameter(Key.plugin, configuration.selectedOptions.toString(false))
        if (!name.isNullOrEmpty()) builder.fragment(name)
        return builder.build()
    }
    override fun toString() = toUri().toString()

    fun serialize() {
        DataStore.privateStore.putString(Key.name, name)
        DataStore.privateStore.putString(Key.host, host)
        DataStore.privateStore.putString(Key.remotePort, remotePort.toString())
        DataStore.privateStore.putString(Key.password, password)
        DataStore.privateStore.putString(Key.route, route)
        DataStore.privateStore.putString(Key.remoteDns, remoteDns)
        DataStore.privateStore.putString(Key.method, method)
        DataStore.proxyApps = proxyApps
        DataStore.bypass = bypass
        DataStore.privateStore.putBoolean(Key.udpdns, udpdns)
        DataStore.privateStore.putBoolean(Key.ipv6, ipv6)
        DataStore.individual = individual
        DataStore.plugin = plugin ?: ""
        DataStore.privateStore.remove(Key.dirty)
    }
    fun deserialize() {
        // It's assumed that default values are never used, so 0/false/null is always used even if that isn't the case
        name = DataStore.privateStore.getString(Key.name) ?: ""
        host = DataStore.privateStore.getString(Key.host) ?: ""
        remotePort = parsePort(DataStore.privateStore.getString(Key.remotePort), 8388, 1)
        password = DataStore.privateStore.getString(Key.password) ?: ""
        method = DataStore.privateStore.getString(Key.method) ?: ""
        route = DataStore.privateStore.getString(Key.route) ?: ""
        remoteDns = DataStore.privateStore.getString(Key.remoteDns) ?: ""
        proxyApps = DataStore.proxyApps
        bypass = DataStore.bypass
        udpdns = DataStore.privateStore.getBoolean(Key.udpdns, false)
        ipv6 = DataStore.privateStore.getBoolean(Key.ipv6, false)
        individual = DataStore.individual
        plugin = DataStore.plugin
    }
}
