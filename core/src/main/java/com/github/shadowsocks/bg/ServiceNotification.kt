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

import android.app.KeyguardManager
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.PowerManager
import android.text.format.Formatter
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import com.github.shadowsocks.Core
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.aidl.TrafficStats
import com.github.shadowsocks.core.R
import com.github.shadowsocks.utils.Action
import com.github.shadowsocks.utils.broadcastReceiver

/**
 * Android < 8 VPN:     always invisible because of VPN notification/icon
 * Android < 8 other:   only invisible in (possibly unsecure) lockscreen
 * Android 8+:          always visible due to system limitations
 *                      (user can choose to hide the notification in secure lockscreen or anywhere)
 */
class ServiceNotification(private val service: BaseService.Interface, profileName: String,
                          channel: String, private val visible: Boolean = false) {
    private val keyGuard = (service as Context).getSystemService<KeyguardManager>()!!
    private val nm by lazy { (service as Context).getSystemService<NotificationManager>()!! }
    private val callback: IShadowsocksServiceCallback by lazy {
        object : IShadowsocksServiceCallback.Stub() {
            override fun stateChanged(state: Int, profileName: String?, msg: String?) { }   // ignore
            override fun trafficUpdated(profileId: Long, stats: TrafficStats) {
                if (profileId != 0L) return
                builder.apply {
                    setContentText((service as Context).getString(R.string.traffic,
                            service.getString(R.string.speed, Formatter.formatFileSize(service, stats.txRate)),
                            service.getString(R.string.speed, Formatter.formatFileSize(service, stats.rxRate))))
                    setSubText(service.getString(R.string.traffic,
                            Formatter.formatFileSize(service, stats.txTotal),
                            Formatter.formatFileSize(service, stats.rxTotal)))
                }
                show()
            }
            override fun trafficPersisted(profileId: Long) { }
        }
    }
    private val lockReceiver = broadcastReceiver { _, intent -> update(intent.action) }
    private var callbackRegistered = false

    private val builder = NotificationCompat.Builder(service as Context, channel)
            .setWhen(0)
            .setColor(ContextCompat.getColor(service, R.color.material_primary_500))
            .setTicker(service.getString(R.string.forward_success))
            .setContentTitle(profileName)
            .setContentIntent(Core.configureIntent(service))
            .setSmallIcon(R.drawable.ic_service_active)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
    private var isVisible = true

    init {
        service as Context
        val closeAction = NotificationCompat.Action.Builder(
                R.drawable.ic_navigation_close,
                service.getString(R.string.stop),
                PendingIntent.getBroadcast(service, 0, Intent(Action.CLOSE), 0)).apply {
            setShowsUserInterface(false)
        }.build()
        if (Build.VERSION.SDK_INT < 24) builder.addAction(closeAction) else builder.addInvisibleAction(closeAction)
        update(if (service.getSystemService<PowerManager>()?.isInteractive != false)
            Intent.ACTION_SCREEN_ON else Intent.ACTION_SCREEN_OFF, true)
        service.registerReceiver(lockReceiver, IntentFilter().apply {
            addAction(Intent.ACTION_SCREEN_ON)
            addAction(Intent.ACTION_SCREEN_OFF)
            if (visible && Build.VERSION.SDK_INT < 26) addAction(Intent.ACTION_USER_PRESENT)
        })
    }

    private fun update(action: String?, forceShow: Boolean = false) {
        if (forceShow || service.data.state == BaseService.State.Connected) when (action) {
            Intent.ACTION_SCREEN_OFF -> {
                setVisible(false, forceShow)
                unregisterCallback()    // unregister callback to save battery
            }
            Intent.ACTION_SCREEN_ON -> {
                setVisible(visible && !keyGuard.isKeyguardLocked, forceShow)
                service.data.binder.registerCallback(callback)
                service.data.binder.startListeningForBandwidth(callback, 1000)
                callbackRegistered = true
            }
            Intent.ACTION_USER_PRESENT -> setVisible(true, forceShow)
        }
    }

    private fun unregisterCallback() {
        if (callbackRegistered) {
            service.data.binder.unregisterCallback(callback)
            callbackRegistered = false
        }
    }

    private fun setVisible(visible: Boolean, forceShow: Boolean = false) {
        if (isVisible != visible) {
            isVisible = visible
            builder.priority = if (visible) NotificationCompat.PRIORITY_LOW else NotificationCompat.PRIORITY_MIN
            show()
        } else if (forceShow) show()
    }

    private fun show() = (service as Service).startForeground(1, builder.build())

    fun destroy() {
        (service as Service).unregisterReceiver(lockReceiver)
        unregisterCallback()
        service.stopForeground(true)
        nm.cancel(1)
    }
}
