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
import com.github.shadowsocks.ShadowsocksApplication.app

/**
  * @author Mygod
  */
class ShadowsocksNotification(private val service: BaseService, profileName: String, visible: Boolean = false) {
  private val keyGuard = service.getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
  private lazy val nm = service.getSystemService(Context.NOTIFICATION_SERVICE).asInstanceOf[NotificationManager]
  private lazy val callback = new Stub {
    override def stateChanged(state: Int, profileName: String, msg: String) = ()  // ignore
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

  private val builder = new NotificationCompat.Builder(service)
    .setWhen(0)
    .setColor(ContextCompat.getColor(service, R.color.material_accent_500))
    .setTicker(service.getString(R.string.forward_success))
    .setContentTitle(profileName)
    .setContentIntent(PendingIntent.getActivity(service, 0, new Intent(service, classOf[Shadowsocks])
      .setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0))
    .setSmallIcon(R.drawable.ic_stat_shadowsocks)
  builder.addAction(R.drawable.ic_navigation_close,
    service.getString(R.string.stop), PendingIntent.getBroadcast(service, 0, new Intent(Action.CLOSE), 0))
  app.profileManager.getAllProfiles match {
    case Some(profiles) => if (profiles.length > 1)
      builder.addAction(R.drawable.ic_action_settings, service.getString(R.string.quick_switch),
        PendingIntent.getActivity(service, 0, new Intent(Action.QUICK_SWITCH), 0))
    case _ =>
  }
  private lazy val style = new BigTextStyle(builder)
  private var isVisible = true
  update(if (service.getSystemService(Context.POWER_SERVICE).asInstanceOf[PowerManager].isScreenOn)
    Intent.ACTION_SCREEN_ON else Intent.ACTION_SCREEN_OFF, true)
  lockReceiver = (context: Context, intent: Intent) => update(intent.getAction)
  val screenFilter = new IntentFilter()
  screenFilter.addAction(Intent.ACTION_SCREEN_ON)
  screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
  if (visible && Utils.isLollipopOrAbove) screenFilter.addAction(Intent.ACTION_USER_PRESENT)
  service.registerReceiver(lockReceiver, screenFilter)

  private def update(action: String, forceShow: Boolean = false) =
    if (forceShow || service.getState == State.CONNECTED) action match {
      case Intent.ACTION_SCREEN_OFF =>
        setVisible(visible && !Utils.isLollipopOrAbove, forceShow)
        unregisterCallback  // unregister callback to save battery
      case Intent.ACTION_SCREEN_ON =>
        setVisible(visible && Utils.isLollipopOrAbove && !keyGuard.inKeyguardRestrictedInputMode, forceShow)
        service.binder.registerCallback(callback)
        callbackRegistered = true
      case Intent.ACTION_USER_PRESENT => setVisible(true, forceShow)
    }

  private def unregisterCallback = if (callbackRegistered) {
    service.binder.unregisterCallback(callback)
    callbackRegistered = false
  }

  def setVisible(visible: Boolean, forceShow: Boolean = false) = if (isVisible != visible) {
    isVisible = visible
    builder.setPriority(if (visible) NotificationCompat.PRIORITY_LOW else NotificationCompat.PRIORITY_MIN)
    show()
  } else if (forceShow) show()

  def show() = service.startForeground(1, builder.build)

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
