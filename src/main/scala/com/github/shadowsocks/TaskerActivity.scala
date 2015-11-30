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

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.Toolbar
import android.util.Log
import android.widget.Switch
import com.github.shadowsocks.helper.TaskerSettings
import com.twofortyfouram.locale.api.{Intent => ApiIntent}

/**
  * @author CzBiX
  */
class TaskerActivity extends AppCompatActivity {
  var taskerOption: TaskerSettings = _

  var switch: Switch = _

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_tasker)

    switch = findViewById(R.id.service_switch).asInstanceOf[Switch]

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.app_name)
    toolbar.setNavigationIcon(R.drawable.ic_close)
    toolbar.setNavigationOnClickListener(_ => finish())
    toolbar.inflateMenu(R.menu.tasker_menu)
    toolbar.setOnMenuItemClickListener(_.getItemId match {
      case R.id.save =>
        saveResult()
        true
      case _ => false
    })

    loadSettings()
  }

  private def loadSettings() {
    val intent: Intent = getIntent
    if (intent.getAction != ApiIntent.ACTION_EDIT_SETTING) {
      Log.w(Shadowsocks.TAG, "unknown tasker action")
      finish()
      return
    }

    taskerOption = TaskerSettings.fromIntent(intent)

    switch.setChecked(taskerOption.is_start)
  }

  private def saveResult() {
    taskerOption.is_start = switch.isChecked
    setResult(Activity.RESULT_OK, taskerOption.toIntent(this))
    finish()
  }
}

