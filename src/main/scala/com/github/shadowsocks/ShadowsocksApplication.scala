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

import android.app.Application
import com.github.shadowsocks.database.DBHelper
import com.google.tagmanager.{Container, ContainerOpener, TagManager}
import com.google.tagmanager.ContainerOpener.{Notifier, OpenType}
import com.google.tagmanager.Container.FunctionCallMacroHandler
import java.util
import com.github.shadowsocks.utils.Utils

class ShadowsocksApplication extends Application {
  lazy val dbHelper = new DBHelper(this)
  lazy val SIG_FUNC = "getSignature"
  var tagContainer: Container = null

  override def onCreate() {
    val tm = TagManager.getInstance(this)
    ContainerOpener.openContainer(tm, "GTM-NT8WS8", OpenType.PREFER_NON_DEFAULT, null, new Notifier {
      override def containerAvailable(container: Container) {
        tagContainer = container
        container.registerFunctionCallMacroHandler(SIG_FUNC, new FunctionCallMacroHandler {
          def getValue(functionName: String, parameters: util.Map[String, AnyRef]): AnyRef = {
            if (functionName == SIG_FUNC) {
              Utils.getSignature(getApplicationContext)
            }
            null
          }
        })
      }
    })
  }
}
