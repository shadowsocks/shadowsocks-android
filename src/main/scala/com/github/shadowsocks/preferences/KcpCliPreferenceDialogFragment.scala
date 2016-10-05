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
