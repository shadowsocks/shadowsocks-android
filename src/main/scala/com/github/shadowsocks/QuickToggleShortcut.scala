package com.github.shadowsocks

import android.app.Activity
import android.content.Intent
import android.content.pm.ShortcutManager
import android.os.{Build, Bundle}
import com.github.shadowsocks.utils.{State, Utils}

/**
  * @author Mygod
  */
class QuickToggleShortcut extends Activity with ServiceBoundContext {
  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    getIntent.getAction match {
      case Intent.ACTION_CREATE_SHORTCUT =>
        setResult(Activity.RESULT_OK, new Intent()
          .putExtra(Intent.EXTRA_SHORTCUT_INTENT, new Intent(this, classOf[QuickToggleShortcut]))
          .putExtra(Intent.EXTRA_SHORTCUT_NAME, getString(R.string.quick_toggle))
          .putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE,
            Intent.ShortcutIconResource.fromContext(this, R.mipmap.ic_launcher)))
        finish()
      case _ =>
        attachService()
        if (Build.VERSION.SDK_INT >= 25) getSystemService(classOf[ShortcutManager]).reportShortcutUsed("toggle")
    }
  }

  override def onDestroy() {
    detachService()
    super.onDestroy()
  }

  override def onServiceConnected() {
    bgService.getState match {
      case State.STOPPED => Utils.startSsService(this)
      case State.CONNECTED => Utils.stopSsService(this)
      case _ => // ignore
    }
    finish()
  }
}
