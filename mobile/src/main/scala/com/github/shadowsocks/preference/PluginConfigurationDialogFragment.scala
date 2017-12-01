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

package com.github.shadowsocks.preference

import android.content.DialogInterface
import android.support.v7.app.AlertDialog
import android.support.v7.preference.EditTextPreferenceDialogFragmentCompat
import android.view.View
import android.widget.EditText
import com.github.shadowsocks.ProfileConfigActivity
import com.github.shadowsocks.plugin.{PluginContract, PluginManager}

/**
  * @author Mygod
  */
object PluginConfigurationDialogFragment {
  final val PLUGIN_ID_FRAGMENT_TAG = "com.github.shadowsocks.preference.PluginConfigurationDialogFragment.PLUGIN_ID"
}

class PluginConfigurationDialogFragment extends EditTextPreferenceDialogFragmentCompat {
  import PluginConfigurationDialogFragment._

  private var editText: EditText = _

  override def onPrepareDialogBuilder(builder: AlertDialog.Builder) {
    super.onPrepareDialogBuilder(builder)
    val intent = PluginManager.buildIntent(getArguments.getString(PLUGIN_ID_FRAGMENT_TAG), PluginContract.ACTION_HELP)
    if (intent.resolveActivity(getActivity.getPackageManager) != null) builder.setNeutralButton("?", ((_, _) =>
      getActivity.startActivityForResult(intent.putExtra(PluginContract.EXTRA_OPTIONS, editText.getText.toString),
        ProfileConfigActivity.REQUEST_CODE_PLUGIN_HELP)): DialogInterface.OnClickListener)
  }

  override def onBindDialogView(view: View) {
    super.onBindDialogView(view)
    editText = view.findViewById(android.R.id.edit)
  }
}
