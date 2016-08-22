package com.github.shadowsocks.utils

import java.io.File

import eu.chainfire.libsuperuser.Shell

import scala.collection.JavaConverters._

/**
  * @author Mygod
  */
object TcpFastOpen {
  /**
    * Is kernel version >= 3.7.1.
    */
  lazy val supported = "^(\\d+)\\.(\\d+)\\.(\\d+)".r.findFirstMatchIn(System.getProperty("os.version")) match {
    case Some(m) =>
      val kernel = m.group(1).toInt
      if (kernel < 3) false else if (kernel > 3) true else {
        val major = m.group(2).toInt
        if (major < 7) false else if (major > 7) true else m.group(3).toInt >= 1
      }
    case _ => false
  }

  def sendEnabled = {
    val file = new File("/proc/sys/net/ipv4/tcp_fastopen")
    file.canRead && (Utils.readAllLines(file).toInt & 1) > 0
  }

  def enabled(value: Boolean) = if (true) {
    val fastopen = "echo " + (if (value) 3 else 0) + " > /proc/sys/net/ipv4/tcp_fastopen"
    Shell.run("su", Array(
      "if " + fastopen + "; then",
      "  success=-1",
      "  if mount -o remount,rw /system; then",
      "    echo '#!/system/bin/sh",
        fastopen + "' > /etc/init.d/tcp_fastopen && chmod 755 /etc/init.d/tcp_fastopen",
      "    success=$?",
      "    mount -o remount,ro /system",
      "  fi",
      "  if [ $success -eq 0 ]; then",
      "    echo Success.",
      "  else",
      "    echo Warning: Unable to create boot script.",
      "  fi",
      "else",
      "  echo Failed.",
      "fi"), null, true).asScala.mkString("\n")
  } else null
}
