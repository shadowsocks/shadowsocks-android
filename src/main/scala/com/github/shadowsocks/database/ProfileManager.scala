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

object ProfileManager {
  private final val TAG = "ProfileManager"
}

class ProfileManager(dbHelper: DBHelper) {
  import ProfileManager._

  var profileAddedListener: Profile => Any = _
  def setProfileAddedListener(listener: Profile => Any) = this.profileAddedListener = listener

  def createProfile(p: Profile = null): Profile = {
    try {
      val profile = if (p == null) new Profile else p
      profile.id = 0
      app.currentProfile match {
        case Some(oldProfile) =>
          // Copy Feature Settings from old profile
          profile.route = oldProfile.route
          profile.ipv6 = oldProfile.ipv6
          profile.proxyApps = oldProfile.proxyApps
          profile.bypass = oldProfile.bypass
          profile.individual = oldProfile.individual
          profile.udpdns = oldProfile.udpdns
        case _ =>
      }
      val last = dbHelper.profileDao.queryRaw(dbHelper.profileDao.queryBuilder.selectRaw("MAX(userOrder)")
        .prepareStatementString).getFirstResult
      if (last != null && last.length == 1 && last(0) != null) profile.userOrder = last(0).toInt + 1
      dbHelper.profileDao.createOrUpdate(profile)
      if (profileAddedListener != null) profileAddedListener(profile)
      profile
    } catch {
      case ex: Exception =>
        Log.e(TAG, "addProfile", ex)
        app.track(ex)
        p
    }
  }

  def updateProfile(profile: Profile): Boolean = {
    try {
      dbHelper.profileDao.update(profile)
      true
    } catch {
      case ex: Exception =>
        Log.e(TAG, "updateProfile", ex)
        app.track(ex)
        false
    }
  }

  def getProfile(id: Int): Option[Profile] = {
    try {
      dbHelper.profileDao.queryForId(id) match {
        case profile: Profile => Option(profile)
        case _ => None
      }
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getProfile", ex)
        app.track(ex)
        None
    }
  }

  def delProfile(id: Int): Boolean = {
    try {
      dbHelper.profileDao.deleteById(id)
      true
    } catch {
      case ex: Exception =>
        Log.e(TAG, "delProfile", ex)
        app.track(ex)
        false
    }
  }

  def getFirstProfile = {
    try {
      val result = dbHelper.profileDao.query(dbHelper.profileDao.queryBuilder.limit(1L).prepare)
      if (result.size == 1) Option(result.get(0)) else None
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getAllProfiles", ex)
        app.track(ex)
        None
    }
  }

  def getAllProfiles: Option[List[Profile]] = {
    try {
      import scala.collection.JavaConversions._
      Option(dbHelper.profileDao.query(dbHelper.profileDao.queryBuilder.orderBy("userOrder", true).prepare).toList)
    } catch {
      case ex: Exception =>
        Log.e(TAG, "getAllProfiles", ex)
        app.track(ex)
        None
    }
  }

  def createDefault(): Profile = {
    val profile = new Profile {
      name = "Default"
      host = "198.199.101.152"
      remotePort = 443
      password = "u1rRWTssNv0p"
    }
    createProfile(profile)
  }
}
