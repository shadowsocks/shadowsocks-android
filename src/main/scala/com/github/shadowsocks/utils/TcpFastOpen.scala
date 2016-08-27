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

  def enabled(value: Boolean) =
    Shell.run("su", Array(
      "if echo " + (if (value) 3 else 0) + " > /proc/sys/net/ipv4/tcp_fastopen; then",
      "  success=-1",
      "  if mount -o rw,remount /dev/block/platform/msm_sdcc.1/by-name/system /system; then",
      if (value) "    echo '#!/system/bin/sh\n" +
        "echo 3 > /proc/sys/net/ipv4/tcp_fastopen' > /etc/init.d/tcp_fastopen && chmod 755 /etc/init.d/tcp_fastopen"
      else "rm -f /etc/init.d/tcp_fastopen",
      "    success=$?",
      "    mount -o ro,remount /dev/block/platform/msm_sdcc.1/by-name/system /system",
      "  fi",
      "  if [ $success -eq 0 ]; then",
      "    echo Success.",
      "  else",
      "    echo Warning: Unable to create boot script.",
      "  fi",
      "else",
      "  echo Failed.",
      "fi"), null, true).asScala.mkString("\n")
}
