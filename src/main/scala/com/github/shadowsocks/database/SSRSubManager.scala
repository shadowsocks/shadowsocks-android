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

package com.github.shadowsocks.database

import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app

object SSRSubManager {
  private final val TAG = "SSRSubManager"
}

class SSRSubManager(dbHelper: DBHelper) {
  import SSRSubManager._

  var ssrsubAddedListener: SSRSub => Any = _
  def setSSRSubAddedListener(listener: SSRSub => Any) = this.ssrsubAddedListener = listener

  def createSSRSub(p: SSRSub = null): SSRSub = {
    val ssrsub = if (p == null) new SSRSub else p
    ssrsub.id = 0
    try {
      dbHelper.ssrsubDao.createOrUpdate(ssrsub)

      if (ssrsubAddedListener != null) ssrsubAddedListener(ssrsub)
    } catch {
      case ex: Exception =>
        Log.e(TAG, "addSSRSub", ex)
        app.track(ex)
    }
    ssrsub
  }

  def updateSSRSub(ssrsub: SSRSub): Boolean = {
    try {
      dbHelper.ssrsubDao.update(ssrsub)
      true
    } catch {
      case ex: Exception =>
        Log.e(TAG, "updateSSRSub", ex)
        app.track(ex)
        false
    }
  }

  def getSSRSub(id: Int): Option[SSRSub] = {
    try {
      dbHelper.ssrsubDao.queryForId(id) match {
        case ssrsub: SSRSub => Option(ssrsub)
        case _ => None
      }
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getSSRSub", ex)
        app.track(ex)
        None
    }
  }

  def delSSRSub(id: Int): Boolean = {
    try {
      dbHelper.ssrsubDao.deleteById(id)
      true
    } catch {
      case ex: Exception =>
        Log.e(TAG, "delSSRSub", ex)
        app.track(ex)
        false
    }
  }

  def getFirstSSRSub = {
    try {
      val result = dbHelper.ssrsubDao.query(dbHelper.ssrsubDao.queryBuilder.limit(1L).prepare)
      if (result.size == 1) Option(result.get(0)) else None
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getAllSSRSubs", ex)
        app.track(ex)
        None
    }
  }

  def getAllSSRSubs: Option[List[SSRSub]] = {
    try {
      import scala.collection.JavaConversions._
      Option(dbHelper.ssrsubDao.query(dbHelper.ssrsubDao.queryBuilder.prepare).toList)
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getAllSSRSubs", ex)
        app.track(ex)
        None
    }
  }

  def createDefault(): SSRSub = {
    val ssrsub = new SSRSub {
      url = "https://raw.githubusercontent.com/breakwa11/breakwa11.github.io/master/free/freenodeplain.txt"
      url_group = "FreeSSR-public"
    }
    createSSRSub(ssrsub)
  }
}
