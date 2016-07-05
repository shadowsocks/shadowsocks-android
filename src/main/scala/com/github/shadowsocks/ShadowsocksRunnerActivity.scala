/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks

import android.app.{Activity, KeyguardManager}
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.net.VpnService
import android.os.{Bundle, Handler}
import android.util.Log
import com.github.shadowsocks.utils.ConfigUtils
import com.github.shadowsocks.ShadowsocksApplication.app

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
    if (app.isNatEnabled) {
      bgService.use(app.profileId)
      finish()
    } else {
      val intent = VpnService.prepare(ShadowsocksRunnerActivity.this)
      if (intent != null) {
        startActivityForResult(intent, REQUEST_CONNECT)
      } else {
        onActivityResult(REQUEST_CONNECT, Activity.RESULT_OK, null)
      }
    }
  }

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    val km = getSystemService(Context.KEYGUARD_SERVICE).asInstanceOf[KeyguardManager]
    val locked = km.inKeyguardRestrictedInputMode
    if (locked) {
      val filter = new IntentFilter(Intent.ACTION_USER_PRESENT)
      receiver = (context: Context, intent: Intent) => {
        if (intent.getAction == Intent.ACTION_USER_PRESENT) {
          attachService()
        }
      }
      registerReceiver(receiver, filter)
    } else {
      attachService()
    }
    finish
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
      case Activity.RESULT_OK =>
        if (bgService != null) {
          bgService.use(app.profileId)
        }
      case _ =>
        Log.e(TAG, "Failed to start VpnService")
    }
    finish()
  }
}
