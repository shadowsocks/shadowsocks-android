package com.github.shadowsocks

import java.util.Locale

import android.app.{KeyguardManager, PendingIntent}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.support.v4.app.NotificationCompat
import android.support.v4.app.NotificationCompat.BigTextStyle
import android.support.v4.content.ContextCompat
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback.Stub
import com.github.shadowsocks.utils.{Action, State, Utils}

/**
  * @author Mygod
  */
class ShadowsocksNotification(private val service: BaseService, profileName: String, visible: Boolean = false,
                              private val showTraffic: Boolean = true) {
  private lazy val keyGuard = service.getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
  private lazy val callback = new Stub {
    override def stateChanged(state: Int, msg: String) = () // ignore
    override def trafficUpdated(txRate: String, rxRate: String, txTotal: String, rxTotal: String) {
      builder.setContentText(service.getString(R.string.traffic_summary).formatLocal(Locale.ENGLISH, txRate, rxRate))
      style.bigText(service.getString(R.string.stat_summary)
        .formatLocal(Locale.ENGLISH, txRate, rxRate, txTotal, rxTotal))
      show()
    }
  }
  private var lockReceiver: BroadcastReceiver = _

  private val builder = new NotificationCompat.Builder(service)
    .setWhen(0)
    .setColor(ContextCompat.getColor(service, R.color.material_accent_500))
    .setTicker(service.getString(R.string.forward_success))
    .setContentTitle(service.getString(R.string.service_running).formatLocal(Locale.ENGLISH, profileName))
    .setContentIntent(PendingIntent.getActivity(service, 0, new Intent(service, classOf[Shadowsocks])
      .setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0))
    .setSmallIcon(R.drawable.ic_stat_shadowsocks)
    .addAction(android.R.drawable.ic_menu_close_clear_cancel, service.getString(R.string.stop),
      PendingIntent.getBroadcast(service, 0, new Intent(Action.CLOSE), 0))
  private lazy val style = new BigTextStyle(builder)
  private val showOnUnlock = visible && Utils.isLollipopOrAbove
  if (showOnUnlock || showTraffic) {
    update(Intent.ACTION_USER_PRESENT)
    lockReceiver = (context: Context, intent: Intent) => update(intent.getAction)
    val screenFilter = new IntentFilter()
    screenFilter.addAction(Intent.ACTION_SCREEN_ON)
    screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
    screenFilter.addAction(Intent.ACTION_USER_PRESENT)
    service.registerReceiver(lockReceiver, screenFilter)
  } else setVisible(visible).show()

  private def update(action: String) = if (service.getState == State.CONNECTED) {
    if (action == Intent.ACTION_SCREEN_OFF) {
      if (showOnUnlock) setVisible(false)
      if (showTraffic) service.binder.unregisterCallback(callback)
    } else if (action == Intent.ACTION_USER_PRESENT ||
      action == Intent.ACTION_SCREEN_ON && !keyGuard.inKeyguardRestrictedInputMode) {
      setVisible(showOnUnlock)
      if (showTraffic) service.binder.registerCallback(callback)
    }
    show()
  }

  def setVisible(visible: Boolean) = {
    builder.setPriority(if (visible) NotificationCompat.PRIORITY_DEFAULT else NotificationCompat.PRIORITY_MIN)
    this
  }

  def notification = if (showTraffic) style.build() else builder.build()
  def show() = service.startForeground(1, notification)

  def destroy() = {
    if (lockReceiver != null) {
      service.unregisterReceiver(lockReceiver)
      lockReceiver = null
    }
    if (showTraffic) service.binder.unregisterCallback(callback)
    service.stopForeground(true)
  }
}
