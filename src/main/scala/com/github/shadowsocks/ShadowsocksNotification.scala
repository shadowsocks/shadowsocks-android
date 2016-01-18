package com.github.shadowsocks

import java.util.Locale

import android.app.{KeyguardManager, NotificationManager, PendingIntent}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.os.PowerManager
import android.support.v4.app.NotificationCompat
import android.support.v4.app.NotificationCompat.BigTextStyle
import android.support.v4.content.ContextCompat
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback.Stub
import com.github.shadowsocks.utils.{TrafficMonitor, Action, State, Utils}

/**
  * @author Mygod
  */
class ShadowsocksNotification(private val service: BaseService, profileName: String, visible: Boolean = false) {
  private val keyGuard = service.getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
  private lazy val nm = service.getSystemService(Context.NOTIFICATION_SERVICE).asInstanceOf[NotificationManager]
  private lazy val callback = new Stub {
    override def stateChanged(state: Int, msg: String) = () // ignore
    override def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
      val txr = TrafficMonitor.formatTraffic(txRate)
      val rxr = TrafficMonitor.formatTraffic(rxRate)
      builder.setContentText(service.getString(R.string.traffic_summary).formatLocal(Locale.ENGLISH, txr, rxr))
      style.bigText(service.getString(R.string.stat_summary).formatLocal(Locale.ENGLISH, txr, rxr,
        TrafficMonitor.formatTraffic(txTotal), TrafficMonitor.formatTraffic(rxTotal)))
      show()
    }
  }
  private var lockReceiver: BroadcastReceiver = _
  private var callbackRegistered: Boolean = _
  private var started: Boolean = _

  private val builder = new NotificationCompat.Builder(service)
    .setWhen(0)
    .setColor(ContextCompat.getColor(service, R.color.material_accent_500))
    .setTicker(service.getString(R.string.forward_success))
    .setContentTitle(profileName)
    .setContentIntent(PendingIntent.getActivity(service, 0, new Intent(service, classOf[Shadowsocks])
      .setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0))
    .setSmallIcon(R.drawable.ic_stat_shadowsocks)
    .addAction(android.R.drawable.ic_menu_close_clear_cancel, service.getString(R.string.stop),
      PendingIntent.getBroadcast(service, 0, new Intent(Action.CLOSE), 0))
  private lazy val style = new BigTextStyle(builder)
  private val showOnUnlock = visible && Utils.isLollipopOrAbove
  private var isVisible = true
  update(if (service.getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager].isScreenOn)
    Intent.ACTION_SCREEN_ON else Intent.ACTION_SCREEN_OFF, true)
  lockReceiver = (context: Context, intent: Intent) => update(intent.getAction)
  val screenFilter = new IntentFilter()
  screenFilter.addAction(Intent.ACTION_SCREEN_ON)
  screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
  if (showOnUnlock) screenFilter.addAction(Intent.ACTION_USER_PRESENT)
  service.registerReceiver(lockReceiver, screenFilter)

  private def update(action: String, forceShow: Boolean = false) =
    if (forceShow || service.getState == State.CONNECTED) action match {
      case Intent.ACTION_SCREEN_OFF =>
        setVisible(false, forceShow)
        unregisterCallback  // unregister callback to save battery
      case Intent.ACTION_SCREEN_ON =>
        setVisible(showOnUnlock && !keyGuard.inKeyguardRestrictedInputMode, forceShow)
        service.binder.registerCallback(callback)
        callbackRegistered = true
      case Intent.ACTION_USER_PRESENT => setVisible(showOnUnlock, forceShow)
    }

  private def unregisterCallback = if (callbackRegistered) {
    service.binder.unregisterCallback(callback)
    callbackRegistered = false
  }

  def setVisible(visible: Boolean, forceShow: Boolean = false) = if (isVisible != visible) {
    isVisible = visible
    builder.setPriority(if (visible) NotificationCompat.PRIORITY_DEFAULT else NotificationCompat.PRIORITY_MIN)
    show()
  } else if (forceShow) show()

  def show() = if (started) nm.notify(1, builder.build) else {
    service.startForeground(1, builder.build)
    started = true
  }

  def destroy() {
    if (lockReceiver != null) {
      service.unregisterReceiver(lockReceiver)
      lockReceiver = null
    }
    unregisterCallback
    service.stopForeground(true)
    nm.cancel(1)
  }
}
