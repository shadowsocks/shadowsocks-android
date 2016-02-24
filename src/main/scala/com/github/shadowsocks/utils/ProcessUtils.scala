package com.github.shadowsocks.utils

import java.io.{BufferedReader, FileInputStream, InputStreamReader}

import com.github.shadowsocks.utils.IOUtils._

/**
  * @author chentaov5@gmail.com
  */
object ProcessUtils {

  val SHADOWNSOCKS     = "com.github.shadowsocks"
  val SHADOWNSOCKS_VPN = "com.github.shadowsocks:vpn"
  val SHADOWNSOCKS_NAT = "com.github.shadowsocks:nat"
  val TUN2SOCKS        = "/data/data/com.github.shadowsocks/tun2socks"
  val SS_LOCAL         = "/data/data/com.github.shadowsocks/ss-local"
  val SS_TUNNEL        = "/data/data/com.github.shadowsocks/ss-tunnel"
  val PDNSD            = "/data/data/com.github.shadowsocks/pdnsd"

  def getCurrentProcessName: String = {
    inSafe {
      val inputStream = new FileInputStream("/proc/self/cmdline")
      val reader = new BufferedReader(new InputStreamReader(inputStream))
      autoClose(reader) {
        reader.readLine() match {
          case name: String => name
          case _ => ""
        }
      }
    } match {
      case Some(name: String) => name
      case None => ""
    }
  }

  def inShadowsocks[A](fun: => A): Unit =
    if(getCurrentProcessName startsWith SHADOWNSOCKS) fun

  def inVpn[A](fun: => A): Unit =
    if (getCurrentProcessName startsWith SHADOWNSOCKS_VPN) fun


  def inNat[A](fun: => A): Unit =
    if (getCurrentProcessName startsWith SHADOWNSOCKS_NAT) fun

}
