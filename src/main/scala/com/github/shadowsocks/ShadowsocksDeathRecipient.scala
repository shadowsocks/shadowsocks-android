package com.github.shadowsocks

import android.os.IBinder
import android.util.Log

/**
  * @author chentaov5@gmail.com
  */
class ShadowsocksDeathRecipient(val mContext: ServiceBoundContext)
  extends IBinder.DeathRecipient {

  val TAG = "ShadowsocksDeathRecipient"

  override def binderDied(): Unit = {
    Log.d(TAG, "[ShadowsocksDeathRecipient] binder died.")
    mContext match {
      case ss: Shadowsocks =>
        ss.deattachService
        ss.crashRecovery
        ss.attachService
      case _ =>
    }
  }
}
