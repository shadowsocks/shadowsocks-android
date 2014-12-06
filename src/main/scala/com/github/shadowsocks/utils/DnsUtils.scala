package com.github.shadowsocks.utils

import java.net.InetAddress

import android.net.{Network, ConnectivityManager}


object DnsUtils {
  def getDnsServers(mgr: ConnectivityManager, network: Network): java.util.List[InetAddress] = {
    val getLinkProperties = mgr.getClass.getMethod("getLinkProperties", network.getClass)
    getLinkProperties.invoke(mgr, network).asInstanceOf[java.util.List[InetAddress]]
  }

  def getNetId(network: Network): Int = {
    network.getClass.getDeclaredField("netId").get(network).asInstanceOf[Int]
  }
}
