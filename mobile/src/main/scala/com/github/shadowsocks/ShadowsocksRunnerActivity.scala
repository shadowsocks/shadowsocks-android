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

package com.github.shadowsocks

import android.app.{Activity, KeyguardManager}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.net.VpnService
import android.os.{Bundle, Handler}
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.Utils

object ShadowsocksRunnerActivity {
  private final val TAG = "ShadowsocksRunnerActivity"
  private final val REQUEST_CONNECT = 1
}

class ShadowsocksRunnerActivity extends Activity with ServiceBoundContext {
  import ShadowsocksRunnerActivity._

  val handler = new Handler()

  // Variables
  var receiver: BroadcastReceiver = _

  override def onServiceConnected() {
    handler.postDelayed(() => if (bgService != null) startBackgroundService(), 1000)
  }

  def startBackgroundService() {
    if (app.usingVpnMode) VpnService.prepare(ShadowsocksRunnerActivity.this) match {
      case null => onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
      case intent => startActivityForResult(intent, REQUEST_CONNECT)
    } else {
      Utils.startSsService(this)
      finish()
    }
  }

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    val km = getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
    val locked = km.inKeyguardRestrictedInputMode
    if (locked) {
      val filter = new IntentFilter(Intent.ACTION_USER_PRESENT)
      receiver = (_: Context, intent: Intent) => {
        if (intent.getAction == Intent.ACTION_USER_PRESENT) {
          attachService()
        }
      }
      registerReceiver(receiver, filter)
    } else {
      attachService()
    }
  }

  override def onDestroy() {
    super.onDestroy()
    detachService()
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK => Utils.startSsService(this)
      case _ =>
        Log.e(TAG, "Failed to start VpnService")
    }
    finish()
  }
}
