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

import android.app.Activity
import android.os.{IBinder, Bundle}
import android.net.VpnService
import android.content.{Context, ComponentName, ServiceConnection, Intent}
import android.util.Log
import android.preference.PreferenceManager
import com.github.shadowsocks.utils._
import com.github.shadowsocks.aidl.IShadowsocksService

class ShadowsocksRunnerActivity extends Activity {

  lazy val settings = PreferenceManager.getDefaultSharedPreferences(this)
  lazy val isRoot = Utils.getRoot

  // Services
  var bgService: IShadowsocksService = null
  val connection = new ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      bgService = IShadowsocksService.Stub.asInterface(service)
      if (!isRoot) {
        val intent = VpnService.prepare(ShadowsocksRunnerActivity.this)
        if (intent != null) {
          startActivityForResult(intent, Shadowsocks.REQUEST_CONNECT)
        } else {
          onActivityResult(Shadowsocks.REQUEST_CONNECT, Activity.RESULT_OK, null)
        }
      } else {
        bgService.start(ConfigUtils.load(settings))
        finish()
      }
    }
    override def onServiceDisconnected(name: ComponentName) {
      bgService = null
    }
  }

  def attachService() {
    if (bgService == null) {
      val s = if (isRoot) classOf[ShadowsocksNatService] else classOf[ShadowsocksVpnService]
      val intent = new Intent(this, s)
      intent.setAction(Action.SERVICE)
      bindService(intent, connection, Context.BIND_AUTO_CREATE)
      startService(new Intent(this, s))
    }
  }

  def deattachService() {
    if (bgService != null) {
      unbindService(connection)
      bgService = null
    }
  }

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    attachService()
  }

  override def onDestroy() {
    super.onDestroy()
    deattachService()
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent) {
    resultCode match {
      case Activity.RESULT_OK =>
        if (bgService != null) {
          bgService.start(ConfigUtils.load(settings))
        }
      case _ =>
        Log.e(Shadowsocks.TAG, "Failed to start VpnService")
    }
    finish()
  }
}
