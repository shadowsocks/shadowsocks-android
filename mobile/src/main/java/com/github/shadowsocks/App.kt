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
import android.support.annotation.RequiresApi
import android.support.v4.os.UserManagerCompat
import android.support.v7.app.AppCompatDelegate
import android.util.Log
import com.evernote.android.job.JobConstants
import com.evernote.android.job.JobManager
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.acl.AclSyncJob
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.BottomSheetPreferenceDialogFragment
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.preference.IconListPreference
import com.github.shadowsocks.utils.*
import com.google.android.gms.analytics.GoogleAnalytics
import com.google.android.gms.analytics.HitBuilders
import com.google.android.gms.analytics.StandardExceptionParser
import com.google.android.gms.analytics.Tracker
import com.google.firebase.FirebaseApp
import com.google.firebase.remoteconfig.FirebaseRemoteConfig
import com.j256.ormlite.logger.LocalLog
import com.takisoft.fix.support.v7.preference.PreferenceFragmentCompat
import java.io.File
import java.io.IOException

class App : Application() {
    companion object {
        lateinit var app: App
        private const val TAG = "ShadowsocksApplication"
    }

    val handler by lazy { Handler(Looper.getMainLooper()) }
    val deviceContext: Context by lazy { if (Build.VERSION.SDK_INT < 24) this else DeviceContext(this) }
    val remoteConfig: FirebaseRemoteConfig by lazy { FirebaseRemoteConfig.getInstance() }
    private val tracker: Tracker by lazy { GoogleAnalytics.getInstance(deviceContext).newTracker(R.xml.tracker) }
    val info: PackageInfo by lazy { packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNATURES) }

    fun startService() {
        val intent = Intent(this, BaseService.serviceClass.java)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
    }
    fun reloadService() = sendBroadcast(Intent(Action.RELOAD))
    fun stopService() = sendBroadcast(Intent(Action.CLOSE))

    val currentProfile: Profile? get() =
        if (DataStore.directBootAware) DirectBoot.getDeviceProfile() else ProfileManager.getProfile(DataStore.profileId)

    fun switchProfile(id: Int): Profile {
        val result = ProfileManager.getProfile(id) ?: ProfileManager.createProfile()
        DataStore.profileId = result.id
        return result
    }

    // send event
    fun track(category: String, action: String) = tracker.send(HitBuilders.EventBuilder()
            .setCategory(category)
            .setAction(action)
            .setLabel(BuildConfig.VERSION_NAME)
            .build())
    fun track(t: Throwable) = track(Thread.currentThread(), t)
    fun track(thread: Thread, t: Throwable) {
        tracker.send(HitBuilders.ExceptionBuilder()
                .setDescription(StandardExceptionParser(this, null).getDescription(thread.name, t))
                .setFatal(false)
                .build())
        t.printStackTrace()
    }

    override fun onCreate() {
        super.onCreate()
        app = this
        if (!BuildConfig.DEBUG) System.setProperty(LocalLog.LOCAL_LOG_LEVEL_PROPERTY, "ERROR")
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true)
        PreferenceFragmentCompat.registerPreferenceFragment(IconListPreference::class.java,
                BottomSheetPreferenceDialogFragment::class.java)

        if (Build.VERSION.SDK_INT >= 24) {  // migrate old files
            deviceContext.moveDatabaseFrom(this, Key.DB_PUBLIC)
            deviceContext.moveDatabaseFrom(this, JobConstants.DATABASE_NAME)
            deviceContext.moveSharedPreferencesFrom(this, JobConstants.PREF_FILE_NAME)
            val old = Acl.getFile(Acl.CUSTOM_RULES, this)
            if (old.canRead()) {
                Acl.getFile(Acl.CUSTOM_RULES).writeText(old.readText())
                old.delete()
            }
        }

        FirebaseApp.initializeApp(deviceContext)
        remoteConfig.setDefaults(R.xml.default_configs)
        remoteConfig.fetch().addOnCompleteListener {
            if (it.isSuccessful) remoteConfig.activateFetched() else Log.e(TAG, "Failed to fetch config")
        }
        JobManager.create(deviceContext).addJobCreator(AclSyncJob)

        // handle data restored
        if (DataStore.directBootAware && UserManagerCompat.isUserUnlocked(this)) DirectBoot.update()
        TcpFastOpen.enabled(DataStore.publicStore.getBoolean(Key.tfo, TcpFastOpen.sendEnabled))
        if (DataStore.publicStore.getLong(Key.assetUpdateTime, -1) != info.lastUpdateTime) {
            val assetManager = assets
            for (dir in arrayOf("acl", "overture"))
                try {
                    for (file in assetManager.list(dir)) assetManager.open(dir + '/' + file).use { input ->
                        File(deviceContext.filesDir, file).outputStream().use { output -> input.copyTo(output) }
                    }
                } catch (e: IOException) {
                    Log.e(TAG, e.message)
                    app.track(e)
                }
            DataStore.publicStore.putLong(Key.assetUpdateTime, info.lastUpdateTime)
        }

        updateNotificationChannels()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        updateNotificationChannels()
    }

    private fun updateNotificationChannels() {
        if (Build.VERSION.SDK_INT >= 26) @RequiresApi(26) {
            val nm = getSystemService(NotificationManager::class.java)
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

    fun listenForPackageChanges(callback: () -> Unit): BroadcastReceiver {
        val filter = IntentFilter(Intent.ACTION_PACKAGE_ADDED)
        filter.addAction(Intent.ACTION_PACKAGE_REMOVED)
        filter.addDataScheme("package")
        val result = broadcastReceiver { _, intent ->
            if (intent.action != Intent.ACTION_PACKAGE_REMOVED ||
                    !intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) callback()
        }
        app.registerReceiver(result, filter)
        return result
    }
}
