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

import android.annotation.TargetApi
import android.net.Uri
import android.os.Parcelable
import android.util.Base64
import android.util.Log
import android.util.LongSparseArray
import androidx.core.net.toUri
import androidx.room.*
import com.github.shadowsocks.plugin.PluginConfiguration
import com.github.shadowsocks.plugin.PluginOptions
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.asIterable
import com.github.shadowsocks.utils.parsePort
import kotlinx.android.parcel.Parcelize
import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import java.io.Serializable
import java.net.URI
import java.net.URISyntaxException
import java.util.*

@Entity
@Parcelize
data class Profile(
        @PrimaryKey(autoGenerate = true)
        var id: Long = 0,
        var name: String? = "",
        var host: String = "198.199.101.152",
        var remotePort: Int = 8388,
        var password: String = "u1rRWTssNv0p",
        var method: String = "aes-256-cfb",
        var route: String = "all",
        var remoteDns: String = "dns.google",
        var proxyApps: Boolean = false,
        var bypass: Boolean = false,
        var udpdns: Boolean = false,
        var ipv6: Boolean = true,
        @TargetApi(28)
        var metered: Boolean = false,
        var individual: String = "",
        var tx: Long = 0,
        var rx: Long = 0,
        var userOrder: Long = 0,
        var plugin: String? = null,
        var udpFallback: Long? = null,

        @Ignore // not persisted in db, only used by direct boot
        var dirty: Boolean = false
) : Parcelable, Serializable {
    companion object {
        private const val TAG = "ShadowParser"
        private const val serialVersionUID = 1L
        private val pattern =
                """(?i)ss://[-a-zA-Z0-9+&@#/%?=.~*'()|!:,;\[\]]*[-a-zA-Z0-9+&@#/%=.~*'()|\[\]]""".toRegex()
        private val userInfoPattern = "^(.+?):(.*)$".toRegex()
        private val legacyPattern = "^(.+?):(.*)@(.+?):(\\d+?)$".toRegex()

        fun findAllUrls(data: CharSequence?, feature: Profile? = null) = pattern.findAll(data ?: "").map {
            val uri = it.value.toUri()
            try {
                if (uri.userInfo == null) {
                    val match = legacyPattern.matchEntire(String(Base64.decode(uri.host, Base64.NO_PADDING)))
                    if (match != null) {
                        val profile = Profile()
                        feature?.copyFeatureSettingsTo(profile)
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
                        feature?.copyFeatureSettingsTo(profile)
                        profile.method = match.groupValues[1]
                        profile.password = match.groupValues[2]
                        // bug in Android: https://code.google.com/p/android/issues/detail?id=192855
                        try {
                            val javaURI = URI(it.value)
                            profile.host = javaURI.host ?: ""
                            if (profile.host.firstOrNull() == '[' && profile.host.lastOrNull() == ']')
                                profile.host = profile.host.substring(1, profile.host.length - 1)
                            profile.remotePort = javaURI.port
                            profile.plugin = uri.getQueryParameter(Key.plugin)
                            profile.name = uri.fragment ?: ""
                            profile
                        } catch (e: URISyntaxException) {
                            Log.e(TAG, "Invalid URI: ${it.value}")
                            null
                        }
                    } else {
                        Log.e(TAG, "Unknown user info: ${it.value}")
                        null
                    }
                }
            } catch (e: IllegalArgumentException) {
                Log.e(TAG, "Invalid base64 detected: ${it.value}")
                null
            }
        }.filterNotNull()

        private class JsonParser(private val feature: Profile? = null) : ArrayList<Profile>() {
            private val fallbackMap = mutableMapOf<Profile, Profile>()

            private fun tryParse(json: JSONObject, fallback: Boolean = false): Profile? {
                val host = json.optString("server")
                if (host.isNullOrEmpty()) return null
                val remotePort = json.optInt("server_port")
                if (remotePort <= 0) return null
                val password = json.optString("password")
                if (password.isNullOrEmpty()) return null
                val method = json.optString("method")
                if (method.isNullOrEmpty()) return null
                return Profile().also {
                    it.host = host
                    it.remotePort = remotePort
                    it.password = password
                    it.method = method
                }.apply {
                    feature?.copyFeatureSettingsTo(this)
                    val id = json.optString("plugin")
                    if (!id.isNullOrEmpty()) {
                        plugin = PluginOptions(id, json.optString("plugin_opts")).toString(false)
                    }
                    name = json.optString("remarks")
                    route = json.optString("route", route)
                    if (fallback) return@apply
                    remoteDns = json.optString("remote_dns", remoteDns)
                    ipv6 = json.optBoolean("ipv6", ipv6)
                    metered = json.optBoolean("metered", metered)
                    json.optJSONObject("proxy_apps")?.also {
                        proxyApps = it.optBoolean("enabled", proxyApps)
                        bypass = it.optBoolean("bypass", bypass)
                        individual = it.optJSONArray("android_list")?.asIterable()?.joinToString("\n") ?: individual
                    }
                    udpdns = json.optBoolean("udpdns", udpdns)
                    json.optJSONObject("udp_fallback")?.let { tryParse(it, true) }?.also { fallbackMap[this] = it }
                }
            }

            fun process(json: Any) {
                when (json) {
                    is JSONObject -> {
                        val profile = tryParse(json)
                        if (profile != null) add(profile) else for (key in json.keys()) process(json.get(key))
                    }
                    is JSONArray -> json.asIterable().forEach(this::process)
                    // ignore other types
                }
            }
            fun finalize(create: (Profile) -> Unit) {
                val profiles = ProfileManager.getAllProfiles() ?: emptyList()
                for ((profile, fallback) in fallbackMap) {
                    val match = profiles.firstOrNull {
                        fallback.host == it.host && fallback.remotePort == it.remotePort &&
                                fallback.password == it.password && fallback.method == it.method &&
                                it.plugin.isNullOrEmpty()
                    }
                    profile.udpFallback = if (match == null) {
                        create(fallback)
                        fallback.id
                    } else match.id
                    ProfileManager.updateProfile(profile)
                }
            }
        }
        fun parseJson(json: String, feature: Profile? = null, create: (Profile) -> Unit) = JsonParser(feature).run {
            process(JSONTokener(json).nextValue())
            for (profile in this) create(profile)
            finalize(create)
        }
    }

    @androidx.room.Dao
    interface Dao {
        @Query("SELECT * FROM `Profile` WHERE `id` = :id")
        operator fun get(id: Long): Profile?

        @Query("SELECT * FROM `Profile` ORDER BY `userOrder`")
        fun list(): List<Profile>

        @Query("SELECT MAX(`userOrder`) + 1 FROM `Profile`")
        fun nextOrder(): Long?

        @Query("SELECT 1 FROM `Profile` LIMIT 1")
        fun isNotEmpty(): Boolean

        @Insert
        fun create(value: Profile): Long

        @Update
        fun update(value: Profile): Int

        @Query("DELETE FROM `Profile` WHERE `id` = :id")
        fun delete(id: Long): Int

        @Query("DELETE FROM `Profile`")
        fun deleteAll(): Int
    }

    val formattedAddress get() = (if (host.contains(":")) "[%s]:%d" else "%s:%d").format(host, remotePort)
    val formattedName get() = if (name.isNullOrEmpty()) formattedAddress else name!!

    fun copyFeatureSettingsTo(profile: Profile) {
        profile.route = route
        profile.ipv6 = ipv6
        profile.metered = metered
        profile.proxyApps = proxyApps
        profile.bypass = bypass
        profile.individual = individual
        profile.udpdns = udpdns
    }

    fun toUri(): Uri {
        val builder = Uri.Builder()
                .scheme("ss")
                .encodedAuthority("%s@%s:%d".format(Locale.ENGLISH,
                        Base64.encodeToString("$method:$password".toByteArray(),
                                Base64.NO_PADDING or Base64.NO_WRAP or Base64.URL_SAFE),
                        if (host.contains(':')) "[$host]" else host, remotePort))
        val configuration = PluginConfiguration(plugin ?: "")
        if (configuration.selected.isNotEmpty())
            builder.appendQueryParameter(Key.plugin, configuration.selectedOptions.toString(false))
        if (!name.isNullOrEmpty()) builder.fragment(name)
        return builder.build()
    }
    override fun toString() = toUri().toString()

    fun toJson(profiles: LongSparseArray<Profile>? = null): JSONObject = JSONObject().apply {
        put("server", host)
        put("server_port", remotePort)
        put("password", password)
        put("method", method)
        if (profiles == null) return@apply
        PluginConfiguration(plugin ?: "").selectedOptions.also {
            if (it.id.isNotEmpty()) {
                put("plugin", it.id)
                put("plugin_opts", it.toString())
            }
        }
        put("remarks", name)
        put("route", route)
        put("remote_dns", remoteDns)
        put("ipv6", ipv6)
        put("metered", metered)
        put("proxy_apps", JSONObject().apply {
            put("enabled", proxyApps)
            if (proxyApps) {
                put("bypass", bypass)
                // android_ prefix is used because package names are Android specific
                put("android_list", JSONArray(individual.split("\n")))
            }
        })
        put("udpdns", udpdns)
        val fallback = profiles.get(udpFallback ?: return@apply)
        if (fallback != null && fallback.plugin.isNullOrEmpty()) fallback.toJson().also { put("udp_fallback", it) }
    }

    fun serialize() {
        DataStore.editingId = id
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
        DataStore.privateStore.putBoolean(Key.metered, metered)
        DataStore.individual = individual
        DataStore.plugin = plugin ?: ""
        DataStore.udpFallback = udpFallback
        DataStore.privateStore.remove(Key.dirty)
    }
    fun deserialize() {
        check(id == 0L || DataStore.editingId == id)
        DataStore.editingId = null
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
        metered = DataStore.privateStore.getBoolean(Key.metered, false)
        individual = DataStore.individual
        plugin = DataStore.plugin
        udpFallback = DataStore.udpFallback
    }
}
