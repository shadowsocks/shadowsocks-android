package com.github.shadowsocks.preferences

import android.app.AlertDialog
import android.content.{Context, DialogInterface}
import android.util.AttributeSet
import android.view.ViewGroup
import eu.chainfire.libsuperuser.Shell

import scala.collection.JavaConversions._

/**
  * @author Mygod
  */
class KcpCliPreference(context: Context, attrs: AttributeSet) extends SummaryEditTextPreference(context, attrs) {
  override def onPrepareDialogBuilder(builder: AlertDialog.Builder) {
    super.onPrepareDialogBuilder(builder)
    builder.setNeutralButton("?", ((_, _) => {
      val dialog = new AlertDialog.Builder(context)
        .setTitle("?")
        .setMessage(asScalaBuffer(Shell.SH.run(context.getApplicationInfo.dataDir + "/kcptun --help"))
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
          .replaceAll("[ ]{2,}", "\n"))
        .show()
      val window = dialog.getWindow
      val attrs = window.getAttributes
      attrs.width = ViewGroup.LayoutParams.MATCH_PARENT
      window.setAttributes(attrs)
    }): DialogInterface.OnClickListener)
  }
}
