package com.github.shadowsocks.bg

import java.io.File
import java.util.Locale

import android.text.TextUtils
import android.util.Log
import com.github.shadowsocks.JniHelper
import com.github.shadowsocks.ShadowsocksApplication.app

import scala.io.Source

/**
  * @author Mygod
  */
object Executable {
  val REDSOCKS = "libredsocks.so"
  val PDNSD = "libpdnsd.so"
  val SS_LOCAL = "libss-local.so"
  val SS_TUNNEL = "libss-tunnel.so"
  val TUN2SOCKS = "libtun2socks.so"
  val OVERTURE = "liboverture.so"

  val EXECUTABLES = Set(SS_LOCAL, SS_TUNNEL, PDNSD, REDSOCKS, TUN2SOCKS, OVERTURE)

  def killAll(): Unit =
    for (process <- new File("/proc").listFiles((_, name) => TextUtils.isDigitsOnly(name))) {
      val exe = new File(Source.fromFile(new File(process, "cmdline")).mkString.split("\0", 2).head)
      if (exe.getParent == app.getApplicationInfo.nativeLibraryDir && EXECUTABLES.contains(exe.getName))
        JniHelper.sigkill(process.getName.toInt) match {
          case 0 =>
          case errno => Log.w("kill", "SIGKILL %s (%d) failed with %d"
            .formatLocal(Locale.ENGLISH, exe.getAbsolutePath, process.getName, errno))
        }
    }
}
