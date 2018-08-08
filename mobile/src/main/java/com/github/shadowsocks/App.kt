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

package com.github.shadowsocks

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.admin.DevicePolicyManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.UserManager
import android.util.Log
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.content.getSystemService
import androidx.work.WorkManager
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.BottomSheetPreferenceDialogFragment
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.IconListPreference
import com.github.shadowsocks.utils.*
import com.google.firebase.FirebaseApp
import com.google.firebase.analytics.FirebaseAnalytics
import com.google.firebase.remoteconfig.FirebaseRemoteConfig
import com.takisoft.preferencex.PreferenceFragmentCompat
import io.fabric.sdk.android.Fabric
import java.io.File
import java.io.IOException

class App : Application() {
    companion object {
        lateinit var app: App
        private const val TAG = "ShadowsocksApplication"
    }

    val handler by lazy { Handler(Looper.getMainLooper()) }
    val deviceStorage by lazy { if (Build.VERSION.SDK_INT < 24) this else DeviceStorageApp(this) }
    val remoteConfig: FirebaseRemoteConfig by lazy { FirebaseRemoteConfig.getInstance() }
    val analytics: FirebaseAnalytics by lazy { FirebaseAnalytics.getInstance(deviceStorage) }
    val info: PackageInfo by lazy { getPackageInfo(packageName) }
    val directBootSupported by lazy {
        Build.VERSION.SDK_INT >= 24 && getSystemService<DevicePolicyManager>()?.storageEncryptionStatus ==
                DevicePolicyManager.ENCRYPTION_STATUS_ACTIVE_PER_USER
    }

    fun getPackageInfo(packageName: String) = packageManager.getPackageInfo(packageName,
            if (Build.VERSION.SDK_INT >= 28) PackageManager.GET_SIGNING_CERTIFICATES
            else @Suppress("DEPRECATION") PackageManager.GET_SIGNATURES)!!

    fun startService() {
        val intent = Intent(this, BaseService.serviceClass.java)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
    }
    fun reloadService() = sendBroadcast(Intent(Action.RELOAD))
    fun stopService() = sendBroadcast(Intent(Action.CLOSE))

    val currentProfile: Profile? get() =
        if (DataStore.directBootAware) DirectBoot.getDeviceProfile() else ProfileManager.getProfile(DataStore.profileId)

    fun switchProfile(id: Long): Profile {
        val result = ProfileManager.getProfile(id) ?: ProfileManager.createProfile()
        DataStore.profileId = result.id
        return result
    }

    override fun onCreate() {
        super.onCreate()
        app = this
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true)
        PreferenceFragmentCompat.registerPreferenceFragment(IconListPreference::class.java,
                BottomSheetPreferenceDialogFragment::class.java)

        if (Build.VERSION.SDK_INT >= 24) {  // migrate old files
            deviceStorage.moveDatabaseFrom(this, Key.DB_PUBLIC)
            val old = Acl.getFile(Acl.CUSTOM_RULES, this)
            if (old.canRead()) {
                Acl.getFile(Acl.CUSTOM_RULES).writeText(old.readText())
                old.delete()
            }
        }

        Fabric.with(deviceStorage, Crashlytics())   // multiple processes needs manual set-up
        FirebaseApp.initializeApp(deviceStorage)
        remoteConfig.setDefaults(R.xml.default_configs)
        remoteConfig.fetch().addOnCompleteListener {
            if (it.isSuccessful) remoteConfig.activateFetched() else {
                Log.e(TAG, "Failed to fetch config")
                Crashlytics.logException(it.exception)
            }
        }
        WorkManager.initialize(deviceStorage, androidx.work.Configuration.Builder().build())

        // handle data restored/crash
        if (Build.VERSION.SDK_INT >= 24 && DataStore.directBootAware &&
                getSystemService<UserManager>()?.isUserUnlocked == true) DirectBoot.flushTrafficStats()
        if (DataStore.tcpFastOpen) TcpFastOpen.enabledAsync(true)
        if (DataStore.publicStore.getLong(Key.assetUpdateTime, -1) != info.lastUpdateTime) {
            val assetManager = assets
            for (dir in arrayOf("acl", "overture"))
                try {
                    for (file in assetManager.list(dir)!!) assetManager.open("$dir/$file").use { input ->
                        File(deviceStorage.filesDir, file).outputStream().use { output -> input.copyTo(output) }
                    }
                } catch (e: IOException) {
                    printLog(e)
                }
            DataStore.publicStore.putLong(Key.assetUpdateTime, info.lastUpdateTime)
        }
        AppCompatDelegate.setDefaultNightMode(DataStore.nightMode)
        updateNotificationChannels()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        updateNotificationChannels()
    }

    private fun updateNotificationChannels() {
        if (Build.VERSION.SDK_INT >= 26) @RequiresApi(26) {
            val nm = getSystemService<NotificationManager>()!!
            nm.createNotificationChannels(listOf(
                    NotificationChannel("service-vpn", getText(R.string.service_vpn),
                            NotificationManager.IMPORTANCE_LOW),
                    NotificationChannel("service-proxy", getText(R.string.service_proxy),
                            NotificationManager.IMPORTANCE_LOW),
                    NotificationChannel("service-transproxy", getText(R.string.service_transproxy),
                            NotificationManager.IMPORTANCE_LOW)))
            nm.deleteNotificationChannel("service-nat") // NAT mode is gone for good
        }
    }

    fun listenForPackageChanges(onetime: Boolean = true, callback: () -> Unit): BroadcastReceiver {
        val filter = IntentFilter(Intent.ACTION_PACKAGE_ADDED)
        filter.addAction(Intent.ACTION_PACKAGE_REMOVED)
        filter.addDataScheme("package")
        val result = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                if (intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) return
                callback()
                if (onetime) app.unregisterReceiver(this)
            }
        }
        app.registerReceiver(result, filter)
        return result
    }
}
