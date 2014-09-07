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
import com.github.shadowsocks.database.DBHelper
import com.github.shadowsocks.utils.Utils
import com.google.android.gms.analytics.GoogleAnalytics
import com.google.android.gms.common.api.ResultCallback
import com.google.android.gms.tagmanager.Container.FunctionCallMacroCallback
import com.google.android.gms.tagmanager.{ContainerHolder, TagManager}

class ShadowsocksApplication extends Application {
  lazy val dbHelper = new DBHelper(this)
  lazy val SIG_FUNC = "getSignature"
  var containerHolder: ContainerHolder = null
  lazy val tracker = GoogleAnalytics.getInstance(this).newTracker(R.xml.tracker)

  override def onCreate() {
    val tm = TagManager.getInstance(this)
    val pending = tm.loadContainerPreferNonDefault("GTM-NT8WS8", R.raw.gtm_default_container)
    val callback = new ResultCallback[ContainerHolder] {
      override def onResult(holder: ContainerHolder): Unit = {
        if (!holder.getStatus.isSuccess) {
          return
        }
        containerHolder = holder
        val container = holder.getContainer
        container.registerFunctionCallMacroCallback(SIG_FUNC, new FunctionCallMacroCallback {
          override def getValue(functionName: String, parameters: util.Map[String, AnyRef]): AnyRef = {
            if (functionName == SIG_FUNC) {
              Utils.getSignature(getApplicationContext)
            }
            null
          }
        })
      }
    }
    pending.setResultCallback(callback, 2, TimeUnit.SECONDS)
  }
}
