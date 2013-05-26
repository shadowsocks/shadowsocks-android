package com.github.shadowsocks

import android.app.Activity
import android.os.Bundle
import android.net.VpnService
import android.content.Intent
import android.util.Log
import android.preference.PreferenceManager

class ShadowVpnActivity extends Activity {

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    val intent = VpnService.prepare(this)
    if (intent != null) {
      startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
    } else {
      onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null)
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK => {
        val intent: Intent = new Intent(this, classOf[ShadowVpnService])
        Extra.put(PreferenceManager.getDefaultSharedPreferences(this), intent)
        startService(intent)
      }
      case _ => {
        Log.e(Shadowsocks.TAG, "Failed to start VpnService")
      }
    }
    finish()
  }
}
