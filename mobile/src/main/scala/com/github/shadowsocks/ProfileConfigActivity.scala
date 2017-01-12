/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks

import android.app.Activity
import android.content.DialogInterface
import android.os.Bundle
import android.support.v7.app.AlertDialog
import android.support.v7.widget.Toolbar
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.Key

class ProfileConfigActivity extends Activity {
  private lazy val child = getFragmentManager.findFragmentById(R.id.content).asInstanceOf[ProfileConfigFragment]

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_profile_config)
    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.profile_config)
    toolbar.setNavigationIcon(R.drawable.ic_navigation_close)
    toolbar.setNavigationOnClickListener(_ => onBackPressed())
    toolbar.inflateMenu(R.menu.profile_config_menu)
    toolbar.setOnMenuItemClickListener(child)
  }

  override def onBackPressed(): Unit = if (app.settings.getBoolean(Key.dirty, false)) new AlertDialog.Builder(this)
    .setTitle(R.string.unsaved_changes_prompt)
    .setPositiveButton(R.string.yes, ((_, _) => child.saveAndExit()): DialogInterface.OnClickListener)
    .setNegativeButton(R.string.no, ((_, _) => finish()): DialogInterface.OnClickListener)
    .setNeutralButton(android.R.string.cancel, null)
    .create()
    .show() else super.onBackPressed()
}
