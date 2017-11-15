/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks.bg

import java.util.Locale

import android.app.{KeyguardManager, NotificationManager, PendingIntent}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.os.{Build, PowerManager}
import android.support.v4.app.NotificationCompat
import android.support.v4.app.NotificationCompat.BigTextStyle
import android.support.v4.content.ContextCompat
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback.Stub
import com.github.shadowsocks.utils.{Action, Utils}
import com.github.shadowsocks.{MainActivity, R}

/**
  * @author Mygod
  */
class ServiceNotification(private val service: BaseService, profileName: String,
                          channel: String, visible: Boolean = false) {
  private val keyGuard = service.getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
  private lazy val nm = service.getSystemService(Context.NOTIFICATION_SERVICE).asInstanceOf[NotificationManager]
  private lazy val callback = new Stub {
    override def stateChanged(state: Int, profileName: String, msg: String): Unit = ()  // ignore
    override def trafficUpdated(profileId: Int, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
      val txr = TrafficMonitor.formatTraffic(txRate)
      val rxr = TrafficMonitor.formatTraffic(rxRate)
      builder.setContentText(service.getString(R.string.traffic_summary).formatLocal(Locale.ENGLISH, txr, rxr))
      style.bigText(service.getString(R.string.stat_summary).formatLocal(Locale.ENGLISH, txr, rxr,
        TrafficMonitor.formatTraffic(txTotal), TrafficMonitor.formatTraffic(rxTotal)))
      show()
    }
    override def trafficPersisted(profileId: Int): Unit = ()
  }
  private var lockReceiver: BroadcastReceiver = _
  private var callbackRegistered: Boolean = _

  private val builder = new NotificationCompat.Builder(service, channel)
    .setWhen(0)
    .setColor(ContextCompat.getColor(service, R.color.material_primary_500))
    .setTicker(service.getString(R.string.forward_success))
    .setContentTitle(profileName)
    .setContentIntent(PendingIntent.getActivity(service, 0, new Intent(service, classOf[MainActivity])
      .setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0))
    .setSmallIcon(R.drawable.ic_stat_shadowsocks)
  if (Build.VERSION.SDK_INT < 24) builder.addAction(R.drawable.ic_navigation_close,
    service.getString(R.string.stop), PendingIntent.getBroadcast(service, 0, new Intent(Action.CLOSE), 0))
  private lazy val style = new BigTextStyle(builder)
  private var isVisible = true
  update(if (service.getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager].isScreenOn)
    Intent.ACTION_SCREEN_ON else Intent.ACTION_SCREEN_OFF, forceShow = true)
  lockReceiver = (_: Context, intent: Intent) => update(intent.getAction)
  val screenFilter = new IntentFilter()
  screenFilter.addAction(Intent.ACTION_SCREEN_ON)
  screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
  if (visible && (21 until 26 contains Build.VERSION.SDK_INT)) screenFilter.addAction(Intent.ACTION_USER_PRESENT)
  service.registerReceiver(lockReceiver, screenFilter)

  private def update(action: String, forceShow: Boolean = false) =
    if (forceShow || service.getState == ServiceState.CONNECTED) action match {
      case Intent.ACTION_SCREEN_OFF =>
        setVisible(visible && Build.VERSION.SDK_INT < 21, forceShow)
        unregisterCallback()  // unregister callback to save battery
      case Intent.ACTION_SCREEN_ON =>
        setVisible(visible && (Build.VERSION.SDK_INT < 21 || !keyGuard.inKeyguardRestrictedInputMode), forceShow)
        service.binder.registerCallback(callback)
        service.binder.startListeningForBandwidth(callback)
        callbackRegistered = true
      case Intent.ACTION_USER_PRESENT => setVisible(visible = true, forceShow = forceShow)
    }

  private def unregisterCallback() = if (callbackRegistered) {
    service.binder.unregisterCallback(callback)
    callbackRegistered = false
  }

  def setVisible(visible: Boolean, forceShow: Boolean = false): Unit = if (isVisible != visible) {
    isVisible = visible
    builder.setPriority(if (visible) NotificationCompat.PRIORITY_LOW else NotificationCompat.PRIORITY_MIN)
    show()
  } else if (forceShow) show()

  def show(): Unit = service.startForeground(1, builder.build)

  def destroy() {
    if (lockReceiver != null) {
      service.unregisterReceiver(lockReceiver)
      lockReceiver = null
    }
    unregisterCallback()
    service.stopForeground(true)
    nm.cancel(1)
  }
}
