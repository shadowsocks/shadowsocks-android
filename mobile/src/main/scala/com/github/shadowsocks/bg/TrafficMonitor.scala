/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks.bg

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
    while (n >= 999.5) {
      n /= 1024
      i = i + 1
    }
    if (i < 0) size + " " + app.getResources.getQuantityString(R.plurals.bytes, size.toInt)
    else numberFormat.format(n) + ' ' + units(i)
  }

  def updateRate(): Boolean = {
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
