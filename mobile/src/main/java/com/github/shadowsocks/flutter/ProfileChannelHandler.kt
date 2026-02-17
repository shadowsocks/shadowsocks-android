package com.github.shadowsocks.flutter

import com.github.shadowsocks.Core
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import java.io.ByteArrayInputStream

object ProfileChannelHandler {
    fun register(messenger: BinaryMessenger) {
        MethodChannel(messenger, "com.github.shadowsocks/profiles").setMethodCallHandler { call, result ->
            GlobalScope.launch(Dispatchers.Main) {
                try {
                    when (call.method) {
                        "getProfiles" -> {
                            val profiles = ProfileManager.getActiveProfiles() ?: emptyList()
                            result.success(profiles.map { it.toFlutterMap() })
                        }
                        "getProfile" -> {
                            val id = (call.argument<Any>("id") as Number).toLong()
                            val profile = ProfileManager.getProfile(id)
                            result.success(profile?.toFlutterMap())
                        }
                        "createProfile" -> {
                            val profile = profileFromFlutterMap(call.arguments as Map<*, *>)
                            val created = ProfileManager.createProfile(profile)
                            result.success(created.toFlutterMap())
                        }
                        "updateProfile" -> {
                            val map = call.arguments as Map<*, *>
                            val id = (map["id"] as Number).toLong()
                            val existing = ProfileManager.getProfile(id)
                            if (existing != null) {
                                updateProfileFromMap(existing, map)
                                ProfileManager.updateProfile(existing)
                                result.success(true)
                            } else {
                                result.success(false)
                            }
                        }
                        "deleteProfile" -> {
                            val id = (call.argument<Any>("id") as Number).toLong()
                            ProfileManager.delProfile(id)
                            result.success(true)
                        }
                        "selectProfile" -> {
                            val id = (call.argument<Any>("id") as Number).toLong()
                            Core.switchProfile(id)
                            result.success(null)
                        }
                        "getSelectedId" -> {
                            result.success(DataStore.profileId)
                        }
                        "importFromText" -> {
                            val text = call.argument<String>("text") ?: ""
                            val profiles = Profile.findAllUrls(text).toList()
                            profiles.forEach { ProfileManager.createProfile(it) }
                            result.success(profiles.size)
                        }
                        "importFromJson" -> {
                            val json = call.argument<String>("json") ?: ""
                            var count = 0
                            Profile.parseJson(json) {
                                count++
                                ProfileManager.createProfile(it)
                            }
                            result.success(count)
                        }
                        "exportToJson" -> {
                            val json = ProfileManager.serializeToJson()
                            result.success(json?.toString(2) ?: "[]")
                        }
                        "getProfileUri" -> {
                            val id = (call.argument<Any>("id") as Number).toLong()
                            val profile = ProfileManager.getProfile(id)
                            result.success(profile?.toUri()?.toString())
                        }
                        "reorderProfiles" -> {
                            @Suppress("UNCHECKED_CAST")
                            val order = call.argument<List<Map<String, Any>>>("order") ?: emptyList()
                            for (item in order) {
                                val id = (item["id"] as Number).toLong()
                                val userOrder = (item["order"] as Number).toLong()
                                ProfileManager.getProfile(id)?.let {
                                    it.userOrder = userOrder
                                    ProfileManager.updateProfile(it)
                                }
                            }
                            result.success(null)
                        }
                        else -> result.notImplemented()
                    }
                } catch (e: Exception) {
                    result.error("PROFILE_ERROR", e.message, null)
                }
            }
        }
    }

    private fun Profile.toFlutterMap(): Map<String, Any?> = mapOf(
        "id" to id,
        "name" to (name ?: ""),
        "host" to host,
        "remotePort" to remotePort,
        "password" to password,
        "method" to method,
        "route" to route,
        "remoteDns" to remoteDns,
        "proxyApps" to proxyApps,
        "bypass" to bypass,
        "udpdns" to udpdns,
        "ipv6" to ipv6,
        "metered" to metered,
        "individual" to individual,
        "plugin" to plugin,
        "udpFallback" to udpFallback,
        "subscription" to subscription.persistedValue,
        "tx" to tx,
        "rx" to rx,
        "userOrder" to userOrder,
    )

    private fun profileFromFlutterMap(map: Map<*, *>): Profile = Profile().apply {
        updateProfileFromMap(this, map)
    }

    private fun updateProfileFromMap(profile: Profile, map: Map<*, *>) {
        (map["name"] as? String)?.let { profile.name = it }
        (map["host"] as? String)?.let { profile.host = it }
        (map["remotePort"] as? Number)?.let { profile.remotePort = it.toInt() }
        (map["password"] as? String)?.let { profile.password = it }
        (map["method"] as? String)?.let { profile.method = it }
        (map["route"] as? String)?.let { profile.route = it }
        (map["remoteDns"] as? String)?.let { profile.remoteDns = it }
        (map["proxyApps"] as? Boolean)?.let { profile.proxyApps = it }
        (map["bypass"] as? Boolean)?.let { profile.bypass = it }
        (map["udpdns"] as? Boolean)?.let { profile.udpdns = it }
        (map["ipv6"] as? Boolean)?.let { profile.ipv6 = it }
        (map["metered"] as? Boolean)?.let { profile.metered = it }
        (map["individual"] as? String)?.let { profile.individual = it }
        (map["plugin"] as? String)?.let { profile.plugin = it }
        (map["udpFallback"] as? Number)?.let { profile.udpFallback = it.toLong() }
    }
}
