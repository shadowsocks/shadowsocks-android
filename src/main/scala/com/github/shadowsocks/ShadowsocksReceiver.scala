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

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.preference.PreferenceManager
import com.github.shadowsocks.utils._

class ShadowsocksReceiver extends BroadcastReceiver {

  val TAG = "Shadowsocks"

  def onReceive(context: Context, intent: Intent) {
    val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(context)
    val status = context.getSharedPreferences(Key.status, Context.MODE_PRIVATE)

    var versionName: String = null
    try {
      versionName = context.getPackageManager.getPackageInfo(context.getPackageName, 0).versionName
    } catch {
      case e: PackageManager.NameNotFoundException =>
        versionName = "NONE"
    }
    val isAutoConnect: Boolean = settings.getBoolean(Key.isAutoConnect, false)
    val isInstalled: Boolean = status.getBoolean(versionName, false)
    if (isAutoConnect && isInstalled) {
      val intent = new Intent(context, classOf[ShadowsocksRunnerActivity])
      intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
      context.startActivity(intent)
    }
  }
}
