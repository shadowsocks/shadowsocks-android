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

package com.github.shadowsocks.utils

import android.net.Uri
import android.util.{Base64, Log}
import com.github.shadowsocks.database.Profile

object Parser {
  val TAG = "ShadowParser"
  private val pattern = "(?i)ss://[-a-zA-Z0-9+&@#/%?=~_|!:,.;]*[-a-zA-Z0-9+&@#/%=~_|]".r
  private val userInfoPattern = "^(.+?):(.*)$".r
  private val legacyPattern = "^((.+?):(.*)@(.+?):(\\d+?))$".r

  def findAll(data: CharSequence): Iterator[Profile] =
    pattern.findAllMatchIn(if (data == null) "" else data).map(m => {
      val uri = Uri.parse(m.matched)
      uri.getUserInfo match {
        case null => new String(Base64.decode(uri.getHost, Base64.NO_PADDING), "UTF-8") match {
          case legacyPattern(_, method, password, host, port) =>  // legacy uri
            val profile = new Profile
            profile.method = method.toLowerCase
            profile.password = password
            profile.host = host
            profile.remotePort = port.toInt
            profile.plugin = uri.getQueryParameter(Key.plugin)
            profile.name = uri.getFragment
            profile
          case _ =>
            Log.e(TAG, "Unrecognized URI: " + m.matched)
            null
        }
        case userInfo =>
          new String(Base64.decode(userInfo, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE), "UTF-8") match {
            case userInfoPattern(method, password) =>
              val profile = new Profile
              profile.method = method
              profile.password = password
              profile.host = uri.getHost
              profile.remotePort = uri.getPort
              profile.plugin = uri.getQueryParameter(Key.plugin)
              profile.name = uri.getFragment
              profile
            case _ =>
              Log.e(TAG, "Unknown user info: " + m.matched)
              null
          }
      }
    }).filter(_ != null)
}
