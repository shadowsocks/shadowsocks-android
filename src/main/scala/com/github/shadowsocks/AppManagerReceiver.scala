package com.github.shadowsocks

import android.content.{Intent, Context, BroadcastReceiver}

/**
  * @author Mygod
  */
class AppManagerReceiver extends BroadcastReceiver {
  override def onReceive(context: Context, intent: Intent) = if (intent.getAction != Intent.ACTION_PACKAGE_REMOVED ||
    !intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) AppManager.cachedApps = null
}
