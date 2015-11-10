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

import android.content.SharedPreferences
import android.util.Log
import com.github.shadowsocks._
import com.github.shadowsocks.utils.Key

class ProfileManager(settings: SharedPreferences, dbHelper: DBHelper) {

  def createOrUpdateProfile(profile: Profile): Boolean = {
    try {
      dbHelper.profileDao.createOrUpdate(profile)
      true
    } catch {
      case ex: Exception =>
        Log.e(Shadowsocks.TAG, "addProfile", ex)
        false
    }
  }

  def updateProfile(profile: Profile): Boolean = {
    try {
      dbHelper.profileDao.update(profile)
      true
    } catch {
      case ex: Exception =>
        Log.e(Shadowsocks.TAG, "addProfile", ex)
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
        Log.e(Shadowsocks.TAG, "getProfile", ex)
        None
    }
  }

  def delProfile(id: Int): Boolean = {
    try {
      dbHelper.profileDao.deleteById(id)
      true
    } catch {
      case ex: Exception =>
        Log.e(Shadowsocks.TAG, "delProfile", ex)
        false
    }
  }

  def getAllProfiles: Option[List[Profile]] = {
    try {
      import scala.collection.JavaConversions._
      Option(dbHelper.profileDao.queryForAll().toList)
    } catch {
      case ex: Exception =>
        Log.e(Shadowsocks.TAG, "getAllProfiles", ex)
        None
    }
  }

  def reload(id: Int): Profile = {
    save()
    load(id)
  }

  def load(id: Int): Profile =  {

    val profile = getProfile(id) getOrElse {
      val p = new Profile()
      createOrUpdateProfile(p)
      p
    }

    val edit = settings.edit()
    edit.putBoolean(Key.isProxyApps, profile.proxyApps)
    edit.putBoolean(Key.isBypassApps, profile.bypass)
    edit.putBoolean(Key.isUdpDns, profile.udpdns)
    edit.putBoolean(Key.isAuth, profile.auth)
    edit.putBoolean(Key.isIpv6, profile.ipv6)
    edit.putString(Key.profileName, profile.name)
    edit.putString(Key.proxy, profile.host)
    edit.putString(Key.sitekey, profile.password)
    edit.putString(Key.encMethod, profile.method)
    edit.putInt(Key.remotePort, profile.remotePort)
    edit.putInt(Key.localPort, profile.localPort)
    edit.putString(Key.proxied, profile.individual)
    edit.putInt(Key.profileId, profile.id)
    edit.putString(Key.route, profile.route)
    edit.apply()

    profile
  }

  private def loadFromPreferences: Profile = {
    val profile = new Profile()

    profile.id = settings.getInt(Key.profileId, -1)

    profile.proxyApps = settings.getBoolean(Key.isProxyApps, false)
    profile.bypass = settings.getBoolean(Key.isBypassApps, false)
    profile.udpdns = settings.getBoolean(Key.isUdpDns, false)
    profile.auth = settings.getBoolean(Key.isAuth, false)
    profile.ipv6 = settings.getBoolean(Key.isIpv6, false)
    profile.name = settings.getString(Key.profileName, "default")
    profile.host = settings.getString(Key.proxy, "127.0.0.1")
    profile.password = settings.getString(Key.sitekey, "default")
    profile.method = settings.getString(Key.encMethod, "table")
    profile.route = settings.getString(Key.route, "all")
    profile.remotePort = settings.getInt(Key.remotePort, 1984)
    profile.localPort = settings.getInt(Key.localPort, 1984)
    profile.individual = settings.getString(Key.proxied, "")

    profile
  }

  def save(): Profile = {
    val profile = loadFromPreferences
    updateProfile(profile)
    profile
  }
}
