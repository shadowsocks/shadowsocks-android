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
import android.content.BroadcastReceiver
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
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.database.SSRSub
import com.github.shadowsocks.database.SSRSubManager
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.readableMessage
import kotlinx.coroutines.*

class SubscriptionService : Service() {
    companion object {
        private const val NOTIFICATION_CHANNEL = "service-subscription"
        private const val NOTIFICATION_ID = 2
        private var worker: Job? = null

        val idle = MutableLiveData<Boolean>(true)

        val notificationChannel @RequiresApi(26) get() = NotificationChannel(NOTIFICATION_CHANNEL,
                app.getText(R.string.service_subscription), NotificationManager.IMPORTANCE_LOW)
    }

    private object CancelReceiver : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            worker?.cancel()
        }
    }

    private var counter = 0
    private var receiverRegistered = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (worker == null) {
            idle.value = false
            if (!receiverRegistered) {
                registerReceiver(CancelReceiver, IntentFilter(Action.ABORT),
                        "$packageName.SERVICE", null)
                receiverRegistered = true
            }
            worker = GlobalScope.launch {
                val ssrsubs = SSRSubManager.getAllSSRSub()
                val notification = NotificationCompat.Builder(this@SubscriptionService, NOTIFICATION_CHANNEL).apply {
                    color = ContextCompat.getColor(this@SubscriptionService, R.color.material_primary_500)
                    priority = NotificationCompat.PRIORITY_LOW
                    addAction(NotificationCompat.Action.Builder(
                            R.drawable.ic_navigation_close,
                            getText(R.string.stop),
                            PendingIntent.getBroadcast(this@SubscriptionService, 0,
                                    Intent(Action.ABORT).setPackage(packageName), 0)).apply {
                        setShowsUserInterface(false)
                    }.build())
                    setCategory(NotificationCompat.CATEGORY_PROGRESS)
                    setContentTitle(getString(R.string.service_subscription_working, 0, ssrsubs.size))
                    setOngoing(true)
                    setProgress(ssrsubs.size, 0, false)
                    setSmallIcon(R.drawable.ic_file_cloud_download)
                    setWhen(0)
                }
                Core.notification.notify(NOTIFICATION_ID, notification.build())
                counter = 0
                val workers = ssrsubs.map {
                    async(Dispatchers.IO) { fetchJson(it, ssrsubs.size, notification) }
                }
                try {
                    val localJsons = workers.awaitAll()
                    withContext(Dispatchers.Main) {
                        Core.notification.notify(NOTIFICATION_ID, notification.apply {
                            setContentTitle(getText(R.string.service_subscription_finishing))
                            setProgress(0, 0, true)
                        }.build())
                        ProfileManager.listener?.reloadProfiles()
                    }
                } finally {
                    for (worker in workers) {
                        worker.cancel()
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

    private suspend fun fetchJson(ssrSub: SSRSub, max: Int, notification: NotificationCompat.Builder) {
        try {
            SSRSubManager.update(ssrSub)
        } catch (e: Exception) {
            ssrSub.status = SSRSub.NETWORK_ERROR
            SSRSubManager.updateSSRSub(ssrSub)
            printLog(e)
            GlobalScope.launch(Dispatchers.Main) {
                Toast.makeText(this@SubscriptionService, e.readableMessage, Toast.LENGTH_LONG).show()
            }
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

    override fun onDestroy() {
        worker?.cancel()
        if (receiverRegistered) unregisterReceiver(CancelReceiver)
        super.onDestroy()
    }
}
