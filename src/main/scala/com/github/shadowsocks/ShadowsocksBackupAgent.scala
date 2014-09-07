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

import android.app.backup.{BackupAgentHelper, FileBackupHelper, SharedPreferencesBackupHelper}
import com.github.shadowsocks.database.DBHelper

class ShadowsocksBackupAgent extends BackupAgentHelper {

  // The names of the SharedPreferences groups that the application maintains.  These
  // are the same strings that are passed to getSharedPreferences(String, int).
  val PREFS_DISPLAY = "com.github.shadowsocks_preferences"

  // An arbitrary string used within the BackupAgentHelper implementation to
  // identify the SharedPreferencesBackupHelper's data.
  val MY_PREFS_BACKUP_KEY = "com.github.shadowsocks"

  val DATABASE = "com.github.shadowsocks.database.profile"

  override def onCreate() {
    val helper = new SharedPreferencesBackupHelper(this, PREFS_DISPLAY)
    addHelper(MY_PREFS_BACKUP_KEY, helper)
    addHelper(DATABASE, new FileBackupHelper(this, "../databases/" + DBHelper.PROFILE))
  }
}
