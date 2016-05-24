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

import android.content.{Context, Intent}
import android.os.Bundle
import com.github.shadowsocks.R
import com.github.shadowsocks.ShadowsocksApplication.app
import com.twofortyfouram.locale.api.{Intent => ApiIntent}

object TaskerSettings {
  private val KEY_SWITCH_ON = "switch_on"
  private val KEY_PROFILE_ID = "profile_id"

  def fromIntent(intent: Intent) = new TaskerSettings(if (intent.hasExtra(ApiIntent.EXTRA_BUNDLE))
    intent.getBundleExtra(ApiIntent.EXTRA_BUNDLE) else Bundle.EMPTY)
}

class TaskerSettings(bundle: Bundle) {
  import TaskerSettings._

  var switchOn = bundle.getBoolean(KEY_SWITCH_ON, true)
  var profileId = bundle.getInt(KEY_PROFILE_ID, -1)

  def toIntent(context: Context) = {
    val bundle = new Bundle()
    if (!switchOn) bundle.putBoolean(KEY_SWITCH_ON, false)
    if (profileId >= 0) bundle.putInt(KEY_PROFILE_ID, profileId)
    new Intent().putExtra(ApiIntent.EXTRA_BUNDLE, bundle).putExtra(ApiIntent.EXTRA_STRING_BLURB,
      app.profileManager.getProfile(profileId) match {
        case Some(p) => context.getString(if (switchOn) R.string.start_service else R.string.stop_service, p.name)
        case None => context.getString(if (switchOn) R.string.start_service_default else R.string.stop)
      })
  }
}

