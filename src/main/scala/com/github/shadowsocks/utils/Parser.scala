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

import android.util.{Base64, Log}
import com.github.shadowsocks.database.Profile

object Parser {
  val TAG = "ShadowParser"
  private val pattern = "(?i)ss://([A-Za-z0-9+-/=_]+)".r
  private val decodedPattern = "(?i)^((.+?)(-auth)??:(.*)@(.+?):(\\d+?))$".r

  def findAll(data: CharSequence) = pattern.findAllMatchIn(if (data == null) "" else data).map(m => try
    decodedPattern.findFirstMatchIn(new String(Base64.decode(m.group(1), Base64.NO_PADDING), "UTF-8")) match {
      case Some(ss) =>
        val profile = new Profile
        profile.method = ss.group(2).toLowerCase
        if (ss.group(3) != null) profile.auth = true
        profile.password = ss.group(4)
        profile.name = ss.group(5)
        profile.host = profile.name
        profile.remotePort = ss.group(6).toInt
        profile
      case _ => null
    }
    catch {
      case ex: Exception =>
        Log.e(TAG, "parser error: " + m.source, ex)// Ignore
        null
    }).filter(_ != null)
}
