package com.github.shadowsocks.utils

import java.text.DecimalFormat

import com.github.shadowsocks.R
import com.github.shadowsocks.ShadowsocksApplication.app

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
  @volatile var dirty = true

  private val units = Array("KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "BB", "NB", "DB", "CB")
  private val numberFormat = new DecimalFormat("@@@")
  def formatTraffic(size: Long): String = {
    var n: Double = size
    var i = -1
    while (n >= 1000) {
      n /= 1024
      i = i + 1
    }
    if (i < 0) size + " " + app.getResources.getQuantityString(R.plurals.bytes, size.toInt)
    else numberFormat.format(n) + ' ' + units(i)
  }

  def updateRate() = {
    val now = System.currentTimeMillis()
    val delta = now - timestampLast
    var updated = false
    if (delta != 0) {
      if (dirty) {
        txRate = (txTotal - txLast) * 1000 / delta
        rxRate = (rxTotal - rxLast) * 1000 / delta
        txLast = txTotal
        rxLast = rxTotal
        dirty = false
        updated = true
      } else {
        if (txRate != 0) {
          txRate = 0
          updated = true
        }
        if (rxRate != 0) {
          rxRate = 0
          updated = true
        }
      }
      timestampLast = now
    }
    updated
  }

  def update(tx: Long, rx: Long) {
    if (txTotal != tx) {
      txTotal = tx
      dirty = true
    }
    if (rxTotal != rx) {
      rxTotal = rx
      dirty = true
    }
  }

  def reset() {
    txRate = 0
    rxRate = 0
    txTotal = 0
    rxTotal = 0
    txLast = 0
    rxLast = 0
    dirty = true
  }
}
