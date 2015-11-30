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
package com.github.shadowsocks.helper

import android.content.{Intent, Context}
import android.os.Bundle
import com.github.shadowsocks.R

import com.twofortyfouram.locale.api.{Intent => ApiIntent}

object TaskerSettings {
  def fromIntent(intent: Intent): TaskerSettings = {
    val bundle: Bundle = if (intent.hasExtra(ApiIntent.EXTRA_BUNDLE))
      intent.getBundleExtra(ApiIntent.EXTRA_BUNDLE) else Bundle.EMPTY

    new TaskerSettings(bundle)
  }

  def fromBundle(bundle: Bundle): TaskerSettings = {
    new TaskerSettings(bundle)
  }
}

class TaskerSettings(bundle: Bundle) {
  val KEY_IS_START = "is_start"

  var is_start: Boolean = bundle.getBoolean(KEY_IS_START)

  def toIntent(context: Context): Intent = {
    val bundle = new Bundle()
    bundle.putBoolean(KEY_IS_START, is_start)

    val desc = context.getString(R.string.turn_service_state,
      context.getString(if (is_start) R.string.state_on else R.string.state_off))

    val intent: Intent = new Intent()
    intent.putExtra(ApiIntent.EXTRA_STRING_BLURB, desc)
      .putExtra(ApiIntent.EXTRA_BUNDLE, bundle)
  }
}

