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

package com.github.shadowsocks.utils

import com.github.shadowsocks.database.Profile
import android.net.Uri
import android.util.{Log, Base64}

object Parser {
  val TAG = "ShadowParser"
  def parse (data: String): Option[Profile] = {
    try {
      Log.d(TAG, data)
      val uri = Uri.parse(data.trim)
      if (uri.getScheme == Scheme.SS) {
        val encoded = data.replace(Scheme.SS + "://", "")
        val content = new String(Base64.decode(encoded, Base64.NO_PADDING), "UTF-8")
        val info = content.split('@')
        val encinfo = info(0).split(':')
        val serverinfo = info(1).split(':')

        val profile = new Profile
        profile.name = serverinfo(0)
        profile.host = serverinfo(0)
        profile.remotePort = serverinfo(1).toInt
        profile.localPort = 1080
        profile.method = encinfo(0)
        profile.password = encinfo(1)
        return Some(profile)
      }
    } catch {
      case ex : Exception => Log.e(TAG, "parser error", ex)// Ignore
    }
    None
  }
}
