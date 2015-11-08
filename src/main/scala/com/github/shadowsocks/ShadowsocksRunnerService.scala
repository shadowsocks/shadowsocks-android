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

import android.app.Service
import android.content._
import android.net.VpnService
import android.os._
import android.preference.PreferenceManager
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.utils._

class ShadowsocksRunnerService extends Service {

  private lazy val settings = PreferenceManager.getDefaultSharedPreferences(this)
  private lazy val status = getSharedPreferences(Key.status, Context.MODE_PRIVATE)

  val handler = new Handler()
  val connection = new ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      bgService = IShadowsocksService.Stub.asInterface(service)
      handler.postDelayed(() => if (bgService != null) startBackgroundService(), 1000)
    }
    override def onServiceDisconnected(name: ComponentName) {
      bgService = null
    }
  }

  // Variables
  var isRoot: Option[Boolean] = None
  var bgService: IShadowsocksService = _

  def isVpnEnabled: Boolean = {
    if (isRoot.isEmpty) isRoot = Some(Console.isRoot)
    !(isRoot.get && status.getBoolean(Key.isNAT, !Utils.isLollipopOrAbove))
  }

  override def onBind(intent: Intent): IBinder = {
    null
  }

  def startBackgroundService() {
    if (isVpnEnabled) {
      val intent = VpnService.prepare(ShadowsocksRunnerService.this)
      if (intent == null) {
        if (bgService != null) {
          bgService.start(ConfigUtils.load(settings))
        }
      }
    } else {
      bgService.start(ConfigUtils.load(settings))
    }
    stopSelf()
  }

  def attachService() {
    if (bgService == null) {
      val s = if (!isVpnEnabled) classOf[ShadowsocksNatService] else classOf[ShadowsocksVpnService]
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

  override def onCreate() {
    super.onCreate()
    attachService()
  }

  override def onDestroy() {
    super.onDestroy()
    deattachService()
  }
}
