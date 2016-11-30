/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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

package com.github.shadowsocks.preferences

import android.app.AlertDialog
import android.content.DialogInterface
import eu.chainfire.libsuperuser.Shell
import be.mygod.preference.EditTextPreferenceDialogFragment

import scala.collection.JavaConverters._

/**
  * @author Mygod
  */
class KcpCliPreferenceDialogFragment extends EditTextPreferenceDialogFragment {
  override def onPrepareDialogBuilder(builder: AlertDialog.Builder) {
    super.onPrepareDialogBuilder(builder)
    builder.setNeutralButton("?", ((_, _) => new AlertDialog.Builder(builder.getContext)
      .setTitle("?")
      .setMessage(Shell.SH.run(builder.getContext.getApplicationInfo.dataDir + "/kcptun --help")
        .asScala
        .dropWhile(line => line != "GLOBAL OPTIONS:")
        .drop(1)
        .takeWhile(line => line.length() > 3)
        .filter(line =>
          !line.startsWith("   --localaddr ") &&
          !line.startsWith("   --remoteaddr ") &&
          !line.startsWith("   --path ") &&
          !line.startsWith("   --help,") &&
          !line.startsWith("   --version,"))
        .mkString("\n")
        .replaceAll(" {2,}", "\n")
        .substring(1))  // remove 1st \n
      .show()): DialogInterface.OnClickListener)
  }
}
