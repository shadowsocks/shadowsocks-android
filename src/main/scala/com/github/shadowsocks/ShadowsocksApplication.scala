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

import java.util
import java.util.concurrent.TimeUnit

import android.app.Application
import android.content.pm.PackageManager
import android.preference.PreferenceManager
import com.j256.ormlite.logger.LocalLog
import com.github.shadowsocks.database.{DBHelper, ProfileManager}
import com.github.shadowsocks.utils.{Key, Utils}
import com.google.android.gms.analytics.{GoogleAnalytics, HitBuilders}
import com.google.android.gms.common.api.ResultCallback
import com.google.android.gms.tagmanager.{ContainerHolder, TagManager}

object ShadowsocksApplication {
  var instance: ShadowsocksApplication = _
  lazy val dbHelper = new DBHelper(instance)
  final val SIG_FUNC = "getSignature"
  var containerHolder: ContainerHolder = _
  lazy val tracker = GoogleAnalytics.getInstance(instance).newTracker(R.xml.tracker)
  lazy val settings = PreferenceManager.getDefaultSharedPreferences(instance)
  lazy val profileManager = new ProfileManager(settings, dbHelper)

  def isVpnEnabled = !settings.getBoolean(Key.isNAT, false)

  def getVersionName = try {
    instance.getPackageManager.getPackageInfo(instance.getPackageName, 0).versionName
  } catch {
    case _: PackageManager.NameNotFoundException => "Package name not found"
    case _: Throwable => null
  }

  // send event
  def track(category: String, action: String) = tracker.send(new HitBuilders.EventBuilder()
    .setAction(action)
    .setLabel(getVersionName)
    .build())

  def profileId = settings.getInt(Key.profileId, -1)
  def profileId(i: Int) = settings.edit.putInt(Key.profileId, i).apply
  def currentProfile = profileManager.getProfile(profileId)

  def switchProfile(id: Int) = {
    profileId(id)
    profileManager.load(id)
  }
}

class ShadowsocksApplication extends Application {
  import ShadowsocksApplication._

  override def onCreate() {
    java.lang.System.setProperty(LocalLog.LOCAL_LOG_LEVEL_PROPERTY, "ERROR");
    ShadowsocksApplication.instance = this
    val tm = TagManager.getInstance(this)
    val pending = tm.loadContainerPreferNonDefault("GTM-NT8WS8", R.raw.gtm_default_container)
    val callback = new ResultCallback[ContainerHolder] {
      override def onResult(holder: ContainerHolder) {
        if (!holder.getStatus.isSuccess) {
          return
        }
        containerHolder = holder
        val container = holder.getContainer
        container.registerFunctionCallMacroCallback(SIG_FUNC,
          (functionName: String, parameters: util.Map[String, AnyRef]) => {
            if (functionName == SIG_FUNC) {
              Utils.getSignature(getApplicationContext)
            }
            null
          })
      }
    }
    pending.setResultCallback(callback, 2, TimeUnit.SECONDS)
  }
}
