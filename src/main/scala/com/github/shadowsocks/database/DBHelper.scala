/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2013 <max.c.lv@gmail.com>
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

package com.github.shadowsocks.database

import com.j256.ormlite.android.apptools.OrmLiteSqliteOpenHelper
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import com.j256.ormlite.support.ConnectionSource
import com.j256.ormlite.table.TableUtils
import com.j256.ormlite.dao.Dao

object DBHelper {
  val PROFILE = "profile.db"
}

class DBHelper(val context: Context)
  extends OrmLiteSqliteOpenHelper(context, DBHelper.PROFILE, null, 9) {

  lazy val profileDao: Dao[Profile, Int] = getDao(classOf[Profile])

  def onCreate(database: SQLiteDatabase, connectionSource: ConnectionSource) {
    TableUtils.createTable(connectionSource, classOf[Profile])
  }

  def onUpgrade(database: SQLiteDatabase, connectionSource: ConnectionSource, oldVersion: Int,
    newVersion: Int) {
    if (oldVersion != newVersion) {
      if (oldVersion < 8) {
        profileDao.executeRaw("ALTER TABLE `profile` ADD COLUMN udpdns SMALLINT;")
        profileDao.executeRaw("ALTER TABLE `profile` ADD COLUMN route VARCHAR;")
      } else if (oldVersion < 9) {
        profileDao.executeRaw("ALTER TABLE `profile` ADD COLUMN route VARCHAR;")
      } else {
        profileDao.executeRaw("DROP TABLE IF EXISTS 'profile';")
        onCreate(database, connectionSource)
      }
    }
  }
}
