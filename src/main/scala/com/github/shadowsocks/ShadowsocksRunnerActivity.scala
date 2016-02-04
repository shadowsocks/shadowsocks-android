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
import android.content.{Intent, IntentFilter, Context, BroadcastReceiver}
import android.net.VpnService
import android.os.{Bundle, Handler}
import android.util.Log
import com.github.shadowsocks.utils.ConfigUtils

class ShadowsocksRunnerActivity extends Activity with ServiceBoundContext {
  val handler = new Handler()

  // Variables
  var receiver: BroadcastReceiver = _

  override def onServiceConnected() {
    handler.postDelayed(() => if (bgService != null) startBackgroundService(), 1000)
  }

  def startBackgroundService() {
    if (ShadowsocksApplication.isVpnEnabled) {
      val intent = VpnService.prepare(ShadowsocksRunnerActivity.this)
      if (intent != null) {
        startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
      } else {
        onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null)
      }
    } else {
      bgService.start(ConfigUtils.load(ShadowsocksApplication.settings))
      finish()
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
    deattachService()
    if (receiver != null) {
      unregisterReceiver(receiver)
      receiver = null
    }
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK =>
        if (bgService != null) {
          bgService.start(ConfigUtils.load(ShadowsocksApplication.settings))
        }
      case _ =>
        Log.e(Shadowsocks.TAG, "Failed to start VpnService")
    }
    finish()
  }
}
