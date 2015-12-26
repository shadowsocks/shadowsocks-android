package com.github.shadowsocks.utils

import java.lang.System
import java.text.DecimalFormat

import com.github.shadowsocks.{R, ShadowsocksApplication}

object TrafficMonitor {
  // Bytes per second
  var txRate: Long = _
  var rxRate: Long = _

  // Bytes for the current session
  var txTotal: Long = _
  var rxTotal: Long = _

  // Bytes for the last query
  var txLast: Long = _
  var rxLast: Long = _
  var timestampLast: Long = _

  private val units = Array("KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "BB", "NB", "DB", "CB")
  private val numberFormat = new DecimalFormat("@@@")
  def formatTraffic(size: Long): String = {
    var n: Double = size
    var i = -1
    while (n >= 1000) {
      n /= 1024
      i = i + 1
    }
    if (i < 0) size + " " + ShadowsocksApplication.instance.getResources.getQuantityString(R.plurals.bytes, size.toInt)
    else numberFormat.format(n) + ' ' + units(i)
  }

  def updateRate() {
    val now = System.currentTimeMillis()
    val delta = now - timestampLast
    if (delta != 0) {
      txRate = (txTotal - txLast) * 1000 / delta
      rxRate = (rxTotal - rxLast) * 1000 / delta
      txLast = txTotal
      rxLast = rxTotal
      timestampLast = now
    }
  }

  def update(tx: Long, rx: Long) {
    txTotal = tx
    rxTotal = rx
  }

  def reset() {
    txRate = 0
    rxRate = 0
    txTotal = 0
    rxTotal = 0
    txLast = 0
    rxLast = 0
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
    formatTraffic(txRate)
  }

  def getRxRate(): String = {
    formatTraffic(rxRate)
  }

  def getRate(): String = {
    formatTraffic(txRate + rxRate)
  }
}

