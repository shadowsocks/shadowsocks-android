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

import android.content.{Context, Intent}
import android.os.Bundle
import android.util.Log
import com.github.shadowsocks.{R, Shadowsocks, ShadowsocksApplication}
import com.twofortyfouram.locale.api.{Intent => ApiIntent}

object TaskerSettings {
  val ACTION_UNKNOWN = "unknown"
  val ACTION_TOGGLE_SERVICE = "toggle_service"
  val ACTION_SWITCH_PROFILE = "switch_profile"

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
  import TaskerSettings._

  private val KEY_ACTION = "action"
  private val KEY_IS_START = "is_start"
  private val KEY_PROFILE_ID = "profile_id"

  var action: String = bundle.getString(KEY_ACTION, ACTION_UNKNOWN)

  var isStart: Boolean = _
  var profileId: Int = _

  action match {
    case ACTION_TOGGLE_SERVICE =>
      isStart = bundle.getBoolean(KEY_IS_START, true)
    case ACTION_SWITCH_PROFILE =>
      profileId = bundle.getInt(KEY_PROFILE_ID, -1)
      assert(profileId != -1, "profile id was wrong")
    case _ =>
      Log.w(Shadowsocks.TAG, s"unknown tasker action settings: $action")
  }

  def isEmpty = action == ACTION_UNKNOWN

  def toIntent(context: Context): Intent = {
    val bundle = new Bundle()

    action match {
      case ACTION_TOGGLE_SERVICE =>
        bundle.putString(KEY_ACTION, action)
        bundle.putBoolean(KEY_IS_START, isStart)
      case ACTION_SWITCH_PROFILE =>
        bundle.putString(KEY_ACTION, action)
        bundle.putInt(KEY_PROFILE_ID, profileId)
    }

    val desc: String = action match {
      case ACTION_TOGGLE_SERVICE =>
        context.getString(R.string.turn_service_state,
          context.getString(if (isStart) R.string.state_on else R.string.state_off))
      case ACTION_SWITCH_PROFILE =>
        val profileName = ShadowsocksApplication.profileManager.getProfile(profileId) match {
          case Some(p) => p.name
          case None => context.getString(R.string.removed)
        }
        context.getString(R.string.switch_profile_to, profileName)
    }

    val intent: Intent = new Intent()
    intent.putExtra(ApiIntent.EXTRA_STRING_BLURB, desc)
      .putExtra(ApiIntent.EXTRA_BUNDLE, bundle)
  }
}

