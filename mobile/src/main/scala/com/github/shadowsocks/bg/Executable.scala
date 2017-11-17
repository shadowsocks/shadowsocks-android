package com.github.shadowsocks.bg

import java.util.Locale
import java.util.concurrent.TimeUnit

import android.util.Log
import android.app.ActivityManager
import android.content.Context
import scala.collection.JavaConversions._

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

  val EXECUTABLES = Array(SS_LOCAL, SS_TUNNEL, PDNSD, REDSOCKS, TUN2SOCKS, OVERTURE)

  def killAll(context: Context) {
    val manager = context.getSystemService(Context.ACTIVITY_SERVICE).asInstanceOf[ActivityManager]

    manager.getRunningAppProcesses.foreach(f => if (f.processName.equals(context.getPackageName + ":bg")) {
      android.os.Process.killProcess(f.pid)
    })
  }
}
