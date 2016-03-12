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

import java.util.Locale

import android.util.{Base64, Log}
import com.github.shadowsocks.database.Profile

object Parser {
  val TAG = "ShadowParser"
  private val pattern = "(?i)ss://([A-Za-z0-9+-/=_]+)".r

  def generate (profile: Profile): String = "ss://" + Base64.encodeToString("%s:%s@%s:%d".formatLocal(Locale.ENGLISH,
    profile.method, profile.password, profile.host, profile.remotePort).getBytes, Base64.NO_PADDING | Base64.NO_WRAP)

  def findAll(data: CharSequence) = pattern.findAllMatchIn(data).map(m => try {
    val content = new String(Base64.decode(m.group(1), Base64.NO_PADDING), "UTF-8")
    val methodIdx = content.indexOf(':')
    val passwordIdx = content.lastIndexOf('@')
    val hostIdx = content.lastIndexOf(':')
    val host = content.substring(passwordIdx + 1, hostIdx)

    val profile = new Profile
    profile.name = host
    profile.host = host
    profile.remotePort = content.substring(hostIdx + 1).toInt
    profile.method = content.substring(0, methodIdx).toLowerCase
    profile.password = content.substring(methodIdx + 1, passwordIdx)
    profile
  } catch {
    case ex: Exception =>
      Log.e(TAG, "parser error: " + m.source, ex)// Ignore
      null
  }).filter(_ != null)
}
