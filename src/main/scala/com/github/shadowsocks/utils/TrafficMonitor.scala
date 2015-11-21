package com.github.shadowsocks.utils

import java.lang.System
import java.text.DecimalFormat

import com.github.shadowsocks.{R, ShadowsocksApplication}

case class Traffic(tx: Long, rx: Long, timestamp: Long)

object TrafficMonitor {
  var last: Traffic = getTraffic(0, 0)

  // Bytes per second
  var txRate: Long = 0
  var rxRate: Long = 0

  // Bytes for the current session
  var txTotal: Long = 0
  var rxTotal: Long = 0

  // Bytes for the last query
  var txLast: Long = 0
  var rxLast: Long = 0

  def getTraffic(tx: Long, rx: Long): Traffic = {
    new Traffic(tx, rx, System.currentTimeMillis())
  }

  private val units = Array("KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "BB", "NB", "DB", "CB")
  private val numberFormat = new DecimalFormat("0.00")
  def formatTraffic(size: Long): String = {
    var n: Double = size
    var i = -1
    while (n >= 1024) {
      n /= 1024
      i = i + 1
    }
    if (i < 0) size + " " + ShadowsocksApplication.instance.getResources.getQuantityString(R.plurals.bytes, size.toInt)
    else numberFormat.format(n) + ' ' + units(i)
  }

  def updateRate() {
    val now = getTraffic(txTotal, rxTotal)
    val delta = now.timestamp - last.timestamp
    val deltaTx = now.tx - last.tx
    val deltaRx = now.rx - last.rx
    if (delta != 0) {
      txRate = deltaTx * 1000 / delta
      rxRate = deltaRx * 1000 / delta
    }
    last = now
  }

  def update(tx: Long, rx: Long) {
    txTotal = tx;
    rxTotal = rx;
  }

  def reset() {
    txRate = 0
    rxRate = 0
    txTotal = 0
    rxTotal = 0
    txLast = 0
    rxLast = 0
    last = getTraffic(0, 0)
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

  def getDeltaTx(): Long = {
    val last = txLast
    txLast = txTotal
    txTotal - last
  }

  def getDeltaRx(): Long = {
    val last = rxLast
    rxLast = rxTotal
    rxTotal - last
  }
}

