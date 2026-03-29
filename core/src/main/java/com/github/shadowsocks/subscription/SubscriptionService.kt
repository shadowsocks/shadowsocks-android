/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2020 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2020 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.subscription

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.IBinder
import android.widget.Toast
import androidx.annotation.RequiresApi
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.MutableLiveData
import com.github.shadowsocks.Core
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.core.R
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.SubscriptionUrls
import com.github.shadowsocks.utils.asIterable
import com.github.shadowsocks.utils.broadcastReceiver
import com.github.shadowsocks.utils.readableMessage
import com.github.shadowsocks.utils.useCancellable
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import timber.log.Timber
import java.io.File
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

class SubscriptionService : Service(), CoroutineScope {
    companion object {
        private const val NOTIFICATION_CHANNEL = "service-subscription"
        private const val NOTIFICATION_ID = 2

        val idle = MutableLiveData(true)

        fun start(context: Context, url: String? = null) {
            val intent = Intent(context, SubscriptionService::class.java)
            if (url != null) intent.putExtra(Action.EXTRA_SUBSCRIPTION_URL, url)
            context.startService(intent)
        }

        val notificationChannel @RequiresApi(26) get() = NotificationChannel(NOTIFICATION_CHANNEL,
                app.getText(R.string.service_subscription), NotificationManager.IMPORTANCE_LOW)
    }

    override val coroutineContext = SupervisorJob() + CoroutineExceptionHandler { _, t -> Timber.w(t) }
    private var worker: Job? = null
    private val cancelReceiver = broadcastReceiver { _, _ -> worker?.cancel() }
    private var counter = 0
    private var receiverRegistered = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (worker == null) {
            idle.value = false
            if (!receiverRegistered) {
                ContextCompat.registerReceiver(this, cancelReceiver, IntentFilter(Action.ABORT),
                    ContextCompat.RECEIVER_NOT_EXPORTED)
                receiverRegistered = true
            }
            worker = launch {
                val urls = intent?.getStringExtra(Action.EXTRA_SUBSCRIPTION_URL)?.let {
                    listOf(SubscriptionUrls.parse(it))
                } ?: Subscription.instance.urls.asIterable().toList()
                val notification = NotificationCompat.Builder(this@SubscriptionService, NOTIFICATION_CHANNEL).apply {
                    color = ContextCompat.getColor(this@SubscriptionService, R.color.material_primary_500)
                    priority = NotificationCompat.PRIORITY_LOW
                    addAction(NotificationCompat.Action.Builder(
                            R.drawable.ic_navigation_close,
                            getText(R.string.stop),
                            PendingIntent.getBroadcast(this@SubscriptionService, 0,
                                    Intent(Action.ABORT).setPackage(packageName), PendingIntent.FLAG_IMMUTABLE)).apply {
                        setShowsUserInterface(false)
                    }.build())
                    setCategory(NotificationCompat.CATEGORY_PROGRESS)
                    setContentTitle(getString(R.string.service_subscription_working, 0, urls.size))
                    setOngoing(true)
                    setProgress(urls.size, 0, false)
                    setSmallIcon(R.drawable.ic_file_cloud_download)
                    setWhen(0)
                }
                Core.notification.notify(NOTIFICATION_ID, notification.build())
                counter = 0
                val workers = urls.map { url -> fetchJsonAsync(url, urls.size, notification) }
                try {
                    val localJsons = workers.awaitAll()
                        withContext(Dispatchers.Main) {
                            Core.notification.notify(NOTIFICATION_ID, notification.apply {
                                setContentTitle(getText(R.string.service_subscription_finishing))
                                setProgress(0, 0, true)
                            }.build())
                            createProfilesFromSubscription(localJsons.asSequence().filterNotNull())
                        }
                } finally {
                    for (worker in workers) {
                        worker.cancel()
                        try {
                            worker.getCompleted()?.second?.apply { if (!delete()) deleteOnExit() }
                        } catch (_: Exception) { }
                    }
                    GlobalScope.launch(Dispatchers.Main) {
                        Core.notification.cancel(NOTIFICATION_ID)
                        idle.value = true
                    }
                    check(worker != null)
                    worker = null
                    stopSelf(startId)
                }
            }
        } else stopSelf(startId)
        return START_NOT_STICKY
    }

    private fun fetchJsonAsync(url: URL, max: Int, notification: NotificationCompat.Builder) = async(Dispatchers.IO) {
        val tempFile = File.createTempFile("subscription-", ".json", cacheDir)
        try {
            (url.openConnection() as HttpURLConnection).useCancellable {
                tempFile.outputStream().use { out -> inputStream.copyTo(out) }
            }
            url to tempFile
        } catch (e: Exception) {
            Timber.d(e)
            launch(Dispatchers.Main) {
                Toast.makeText(this@SubscriptionService, e.readableMessage, Toast.LENGTH_LONG).show()
            }
            if (!tempFile.delete()) tempFile.deleteOnExit()
            null
        } finally {
            withContext(Dispatchers.Main) {
                counter += 1
                Core.notification.notify(NOTIFICATION_ID, notification.apply {
                    setContentTitle(getString(R.string.service_subscription_working, counter, max))
                    setProgress(max, counter, false)
                }.build())
            }
        }
    }

    private fun createProfilesFromSubscription(jsons: Sequence<Pair<URL, File>>) {
        val entries = jsons.toList()
        val currentId = DataStore.profileId
        val profiles = ProfileManager.getAllProfiles()
        val targetUrls = entries.map { it.first.toString() }.toSet()
        val subscriptions = mutableMapOf<Triple<String?, String?, String>, Profile>()
        val toUpdate = mutableSetOf<Long>()
        val preferredProfiles = mutableMapOf<String, Profile>()
        var feature: Profile? = null
        profiles?.forEach { profile ->  // preprocessing phase
            if (currentId == profile.id) feature = profile
            if (profile.subscription == Profile.SubscriptionStatus.UserConfigured) return@forEach
            if (profile.subscriptionUrl !in targetUrls) return@forEach
            if (entries.size == 1 && currentId == profile.id && profile.subscriptionUrl != null) {
                preferredProfiles[profile.subscriptionUrl!!] = profile
            }
            if (subscriptions.putIfAbsent(
                            Triple(profile.subscriptionUrl, profile.name, profile.formattedAddress), profile) != null) {
                ProfileManager.delProfile(profile.id)
                if (currentId == profile.id) DataStore.profileId = 0
            } else if (profile.subscription == Profile.SubscriptionStatus.Active) {
                toUpdate.add(profile.id)
                profile.subscription = Profile.SubscriptionStatus.Obsolete
            }
        }

        for ((url, file) in entries) try {
            val sourceUrl = url.toString()
            Profile.parseJson(file.inputStream().bufferedReader().readText(), feature) {
                it.subscriptionUrl = sourceUrl
                subscriptions.compute(Triple(sourceUrl, it.name, it.formattedAddress)) { _, oldProfile ->
                    when (oldProfile?.subscription) {
                        Profile.SubscriptionStatus.Active -> {
                            feature?.copyFeatureSettingsTo(oldProfile)
                            Timber.w("Duplicate profiles detected. Please use different profile names and/or " +
                                    "address:port for better subscription support.")
                            oldProfile
                        }
                        Profile.SubscriptionStatus.Obsolete -> {
                            updateSubscriptionProfile(oldProfile, it, sourceUrl, feature, toUpdate)
                        }
                        else -> {
                            val preferredProfile = preferredProfiles.remove(sourceUrl)
                            if (preferredProfile != null && preferredProfile.subscription != Profile.SubscriptionStatus.UserConfigured) {
                                updateSubscriptionProfile(preferredProfile, it, sourceUrl, feature, toUpdate)
                            } else {
                                ProfileManager.createProfile(it.apply {
                                    subscription = Profile.SubscriptionStatus.Active
                                })
                            }
                        }
                    }
                }!!
            }
        } catch (e: Exception) {
            Timber.d(e)
            Toast.makeText(this, e.readableMessage, Toast.LENGTH_LONG).show()
        }

        profiles?.forEach { profile -> if (toUpdate.contains(profile.id)) ProfileManager.updateProfile(profile) }
        ProfileManager.listener?.reloadProfiles()
    }

    private fun updateSubscriptionProfile(
            target: Profile,
            source: Profile,
            sourceUrl: String,
            feature: Profile?,
            toUpdate: MutableSet<Long>,
    ): Profile {
        toUpdate.add(target.id)
        feature?.copyFeatureSettingsTo(target)
        target.name = source.name
        target.host = source.host
        target.remotePort = source.remotePort
        target.password = source.password
        target.method = source.method
        target.plugin = source.plugin
        target.udpFallback = source.udpFallback
        target.subscriptionUrl = sourceUrl
        target.subscription = Profile.SubscriptionStatus.Active
        if (DataStore.profileId == 0L) DataStore.profileId = target.id
        return target
    }

    override fun onDestroy() {
        cancel()
        if (receiverRegistered) unregisterReceiver(cancelReceiver)
        super.onDestroy()
    }
}
