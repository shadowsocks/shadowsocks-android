package com.github.shadowsocks.bg

import java.util.Locale
import java.util.concurrent.TimeUnit

import android.util.Log

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

  def killAll() {
    val killer = new ProcessBuilder("killall" +: EXECUTABLES: _*).start()
    if (!killer.waitFor(1, TimeUnit.SECONDS))
      Log.w("killall", "%s didn't exit within 1s. Post-crash clean-up may have failed."
        .formatLocal(Locale.ENGLISH, killer.toString))
  }
}
