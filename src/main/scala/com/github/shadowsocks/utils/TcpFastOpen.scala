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

  def enabled(value: Boolean): String = if (sendEnabled != value) {
    val suAvailable = Shell.SU.available();
    if (suAvailable == true) {
      val res = Shell.run("su", Array(
        "if echo " + (if (value) 3 else 0) + " > /proc/sys/net/ipv4/tcp_fastopen; then",
        "  echo Success.",
        "else",
        "  echo Failed.",
        "fi"), null, true)
      if (res != null) res.asScala.mkString("\n") else null
    } else null
  } else null
}
