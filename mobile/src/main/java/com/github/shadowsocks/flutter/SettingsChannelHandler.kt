package com.github.shadowsocks.flutter

import com.github.shadowsocks.BootReceiver
import com.github.shadowsocks.Core
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.DirectBoot
import com.github.shadowsocks.utils.Key
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel

object SettingsChannelHandler {
    fun register(messenger: BinaryMessenger) {
        MethodChannel(messenger, "com.github.shadowsocks/settings").setMethodCallHandler { call, result ->
            try {
                when (call.method) {
                    "getServiceMode" -> result.success(DataStore.serviceMode)
                    "setServiceMode" -> {
                        val mode = call.argument<String>("mode") ?: Key.modeVpn
                        DataStore.publicStore.putString(Key.serviceMode, mode)
                        result.success(null)
                    }
                    "getPortProxy" -> result.success(DataStore.portProxy)
                    "setPortProxy" -> {
                        DataStore.portProxy = call.argument<Int>("port") ?: 1080
                        result.success(null)
                    }
                    "getPortLocalDns" -> result.success(DataStore.portLocalDns)
                    "setPortLocalDns" -> {
                        DataStore.portLocalDns = call.argument<Int>("port") ?: 5450
                        result.success(null)
                    }
                    "getPortTransproxy" -> result.success(DataStore.portTransproxy)
                    "setPortTransproxy" -> {
                        DataStore.portTransproxy = call.argument<Int>("port") ?: 8200
                        result.success(null)
                    }
                    "getPersistAcrossReboot" -> result.success(DataStore.persistAcrossReboot)
                    "setPersistAcrossReboot" -> {
                        val value = call.argument<Boolean>("value") ?: false
                        DataStore.publicStore.putBoolean(Key.persistAcrossReboot, value)
                        BootReceiver.enabled = value
                        result.success(null)
                    }
                    "getDirectBootAware" -> result.success(DataStore.directBootAware)
                    "setDirectBootAware" -> {
                        val value = call.argument<Boolean>("value") ?: false
                        DataStore.publicStore.putBoolean(Key.directBootAware, value)
                        if (value) DirectBoot.update() else DirectBoot.clean()
                        result.success(null)
                    }
                    "getVersion" -> result.success(Core.packageInfo.versionName)
                    "getString" -> {
                        val key = call.argument<String>("key") ?: ""
                        result.success(DataStore.publicStore.getString(key))
                    }
                    "putString" -> {
                        val key = call.argument<String>("key") ?: ""
                        val value = call.argument<String>("value") ?: ""
                        DataStore.publicStore.putString(key, value)
                        result.success(null)
                    }
                    "getBool" -> {
                        val key = call.argument<String>("key") ?: ""
                        result.success(DataStore.publicStore.getBoolean(key, false))
                    }
                    "putBool" -> {
                        val key = call.argument<String>("key") ?: ""
                        val value = call.argument<Boolean>("value") ?: false
                        DataStore.publicStore.putBoolean(key, value)
                        result.success(null)
                    }
                    else -> result.notImplemented()
                }
            } catch (e: Exception) {
                result.error("SETTINGS_ERROR", e.message, null)
            }
        }
    }
}
