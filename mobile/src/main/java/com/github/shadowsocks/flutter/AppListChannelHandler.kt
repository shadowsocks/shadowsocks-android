package com.github.shadowsocks.flutter

import android.Manifest
import android.content.Context
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import com.github.shadowsocks.preference.DataStore
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream

object AppListChannelHandler {
    fun register(messenger: BinaryMessenger, context: Context) {
        MethodChannel(messenger, "com.github.shadowsocks/applist").setMethodCallHandler { call, result ->
            GlobalScope.launch(Dispatchers.Main) {
                try {
                    when (call.method) {
                        "getApps" -> {
                            val pm = context.packageManager
                            val proxiedSet = DataStore.individual.split("\n").toSet()
                            val apps = pm.getInstalledApplications(PackageManager.GET_META_DATA)
                                .filter { ai ->
                                    try {
                                        pm.getPackageInfo(ai.packageName, PackageManager.GET_PERMISSIONS)
                                            ?.requestedPermissions
                                            ?.contains(Manifest.permission.INTERNET) == true
                                    } catch (_: PackageManager.NameNotFoundException) {
                                        false
                                    }
                                }
                                .map { ai ->
                                    mapOf(
                                        "package" to ai.packageName,
                                        "name" to (pm.getApplicationLabel(ai)?.toString() ?: ai.packageName),
                                        "uid" to ai.uid,
                                        "isProxied" to proxiedSet.contains(ai.packageName),
                                    )
                                }
                            result.success(apps)
                        }
                        "getAppIcon" -> {
                            val packageName = call.argument<String>("package") ?: ""
                            val pm = context.packageManager
                            try {
                                val drawable = pm.getApplicationIcon(packageName)
                                val bitmap = if (drawable is BitmapDrawable) {
                                    drawable.bitmap
                                } else {
                                    val bmp = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888)
                                    val canvas = Canvas(bmp)
                                    drawable.setBounds(0, 0, 48, 48)
                                    drawable.draw(canvas)
                                    bmp
                                }
                                val stream = ByteArrayOutputStream()
                                bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)
                                result.success(stream.toByteArray())
                            } catch (_: PackageManager.NameNotFoundException) {
                                result.success(null)
                            }
                        }
                        "getProxyAppsConfig" -> {
                            result.success(mapOf(
                                "enabled" to DataStore.proxyApps,
                                "bypass" to DataStore.bypass,
                                "individual" to DataStore.individual,
                            ))
                        }
                        "setProxiedApps" -> {
                            @Suppress("UNCHECKED_CAST")
                            val packages = call.argument<List<String>>("packages") ?: emptyList()
                            val bypass = call.argument<Boolean>("bypass") ?: false
                            DataStore.individual = packages.joinToString("\n")
                            DataStore.bypass = bypass
                            DataStore.proxyApps = packages.isNotEmpty()
                            result.success(null)
                        }
                        else -> result.notImplemented()
                    }
                } catch (e: Exception) {
                    result.error("APPLIST_ERROR", e.message, null)
                }
            }
        }
    }
}
