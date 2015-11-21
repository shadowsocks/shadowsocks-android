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
import android.content.Intent
import android.net.VpnService
import android.os.{IBinder, Handler}
import com.github.shadowsocks.utils.ConfigUtils

class ShadowsocksRunnerService extends Service with ServiceBoundContext {
  val handler = new Handler()

  override def onBind(intent: Intent): IBinder = {
    null
  }

  override def onServiceConnected() {
    handler.postDelayed(() => if (bgService != null) startBackgroundService(), 1000)
  }

  def startBackgroundService() {
    if (ShadowsocksApplication.isVpnEnabled) {
      val intent = VpnService.prepare(ShadowsocksRunnerService.this)
      if (intent == null) {
        if (bgService != null) {
          bgService.start(ConfigUtils.load(ShadowsocksApplication.settings))
        }
      }
    } else {
      bgService.start(ConfigUtils.load(ShadowsocksApplication.settings))
    }
    stopSelf()
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
