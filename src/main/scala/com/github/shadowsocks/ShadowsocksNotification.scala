package com.github.shadowsocks

import java.util.Locale

import android.app.{KeyguardManager, PendingIntent}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.support.v4.app.NotificationCompat
import android.support.v4.app.NotificationCompat.BigTextStyle
import android.support.v4.content.ContextCompat
import com.github.shadowsocks.utils.{Action, State, Utils}

/**
  * @author Mygod
  */
class ShadowsocksNotification(private val service: BaseService, profileName: String, visible: Boolean = false) {
  private lazy val keyGuard = service.getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
  private var lockReceiver: BroadcastReceiver = _

  private val builder = new NotificationCompat.Builder(service)
    .setWhen(0)
    .setColor(ContextCompat.getColor(service, R.color.material_accent_500))
    .setTicker(service.getString(R.string.forward_success))
    .setContentTitle(service.getString(R.string.app_name))
    .setContentText(service.getString(R.string.service_running).formatLocal(Locale.ENGLISH, profileName))
    .setContentIntent(PendingIntent.getActivity(service, 0, new Intent(service, classOf[Shadowsocks])
      .setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT), 0))
    .setSmallIcon(R.drawable.ic_stat_shadowsocks)
    .addAction(android.R.drawable.ic_menu_close_clear_cancel, service.getString(R.string.stop),
      PendingIntent.getBroadcast(service, 0, new Intent(Action.CLOSE), 0))
  private val style = new BigTextStyle(builder)
  if (visible && Utils.isLollipopOrAbove) {
    val screenFilter = new IntentFilter()
    screenFilter.addAction(Intent.ACTION_SCREEN_ON)
    screenFilter.addAction(Intent.ACTION_SCREEN_OFF)
    screenFilter.addAction(Intent.ACTION_USER_PRESENT)
    lockReceiver = (context: Context, intent: Intent) => if (service.getState == State.CONNECTED) {
      val action = intent.getAction
      if (action == Intent.ACTION_SCREEN_OFF) {
        setVisible(false).show()
      } else if (action == Intent.ACTION_SCREEN_ON) {
        if (!keyGuard.inKeyguardRestrictedInputMode) {
          setVisible(true).show()
        }
      } else if (action == Intent.ACTION_USER_PRESENT) {
        setVisible(true).show()
      }
    }
    service.registerReceiver(lockReceiver, screenFilter)
  }
  setVisible(visible).show()

  def setVisible(visible: Boolean) = {
    builder.setPriority(if (visible) NotificationCompat.PRIORITY_DEFAULT else NotificationCompat.PRIORITY_MIN)
    this
  }

  def notification = style.build()
  def show() = service.startForeground(1, notification)

  def destroy() = {
    if (lockReceiver != null) {
      service.unregisterReceiver(lockReceiver)
      lockReceiver = null
    }
    service.stopForeground(true)
  }
}
