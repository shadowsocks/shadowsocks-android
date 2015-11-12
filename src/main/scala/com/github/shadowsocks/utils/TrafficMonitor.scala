package com.github.shadowsocks.utils

import android.net.TrafficStats
import android.os.Process
import java.lang.{String, System}
import java.util.Locale

case class Traffic(tx: Long, rx: Long, timestamp: Long)

object TrafficMonitor {
  val uid = Process.myUid
  var last: Traffic = getTraffic

  // Kilo bytes per second
  var txRate: Long = 0
  var rxRate: Long = 0

  // Kilo bytes for the current session
  var txTotal: Long = 0
  var rxTotal: Long = 0

  def getTraffic(): Traffic = {
    new Traffic(TrafficStats.getUidTxBytes(uid),
      TrafficStats.getUidRxBytes(uid), System.currentTimeMillis())
  }

  def formatTraffic(n: Long): String {
    if (n <= 1024) {
      "%d KB".formatLocal(Locale.ENGLISH, n)
    } else if (n > 1024) {
      "%d MB".formatLocal(Locale.ENGLISH, n / 1024)
    } else if (n > 1024 * 1024) {
      "%d GB".formatLocal(Locale.ENGLISH, n / 1024 / 1024)
    } else if (n > 1024 * 1024 * 1024) {
      "%d TB".formatLocal(Locale.ENGLISH, n / 1024 / 1024 / 1024)
    } else  {
      ">1024 TB"
    }
  }

  def updateTraffic() {
    val now = getTraffic
    txRate = ((now.tx - last.tx) / 1024 / (now.timestamp - last.timestamp))
    rxRate = ((now.rx - last.rx) / 1024 / (now.timestamp - last.timestamp))
    txTotal += (now.tx - last.tx) / 1024
    rxTotal += (now.rx - last.rx) / 1024
    last = now
  }

  def resetTraffic() {
    txRate = 0
    rxRate = 0
    txTotal = 0
    rxTotal = 0
    last = getTraffic
  }

  def getTxTotal(): String = {
    formatTraffic(txTotal)
  }

  def getRxTotal(): String = {
    formatTraffic(rxTotal)
  }

  def getTotal(): String = {
    formatTraffic(txTotal + rxTotal)
  }

  def getTxRate(): String = {
    formatTraffic(txRate) + "/s"
  }

  def getTxRate(): String = {
    formatTraffic(rxRate) + "/s"
  }

  def getRate(): String = {
    formatTraffic(txRate + rxRate) + "/s"
  }
}

class TrafficMonitor ()
