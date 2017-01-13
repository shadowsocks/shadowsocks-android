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

import android.app.{Activity, AlertDialog}
import android.content.{DialogInterface, Intent}
import be.mygod.preference.EditTextPreferenceDialogFragment
import com.github.shadowsocks.plugin.PluginInterface

/**
  * @author Mygod
  */
object PluginConfigurationDialogFragment {
  final val PLUGIN_ID_FRAGMENT_TAG = "com.github.shadowsocks.preference.PluginConfigurationDialogFragment.PLUGIN_ID"
  private final val REQUEST_CODE_HELP = 1
}

class PluginConfigurationDialogFragment extends EditTextPreferenceDialogFragment {
  import PluginConfigurationDialogFragment._

  override def onPrepareDialogBuilder(builder: AlertDialog.Builder) {
    super.onPrepareDialogBuilder(builder)
    val intent = new Intent(PluginInterface.ACTION_HELP(getArguments.getString(PLUGIN_ID_FRAGMENT_TAG)))
    if (intent.resolveActivity(getContext.getPackageManager) != null) builder.setNeutralButton("?", ((_, _) =>
      startActivityForResult(intent.putExtra(PluginInterface.EXTRA_OPTIONS, editText.getText.toString),
        REQUEST_CODE_HELP)): DialogInterface.OnClickListener)
  }

  override def onActivityResult(requestCode: Int, resultCode: Int, data: Intent): Unit = requestCode match {
    case REQUEST_CODE_HELP => requestCode match {
      case Activity.RESULT_OK => new AlertDialog.Builder(getContext)
        .setTitle("?")
        .setMessage(data.getCharSequenceExtra(PluginInterface.EXTRA_HELP_MESSAGE))
        .show()
      case _ =>
    }
    case _ => super.onActivityResult(requestCode, resultCode, data)
  }
}
