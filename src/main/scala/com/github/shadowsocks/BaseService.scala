/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks

import java.util.Locale

import android.app.Notification
import android.content.Context
import android.os.{Handler, RemoteCallbackList}
import com.github.shadowsocks.aidl.{Config, IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.utils.{State, TrafficMonitor, TrafficMonitorThread}

trait BaseService {

  @volatile private var state = State.INIT
  @volatile private var callbackCount = 0
  @volatile private var trafficMonitorThread: TrafficMonitorThread = null
  var config: Config = null

  final val callbacks = new RemoteCallbackList[IShadowsocksServiceCallback]

  protected val binder = new IShadowsocksService.Stub {
    override def getMode: Int = {
      getServiceMode
    }

    override def getState: Int = {
      state
    }

    override def unregisterCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null ) {
        callbacks.unregister(cb)
        callbackCount -= 1
      }
      if (callbackCount == 0 && state != State.CONNECTING && state != State.CONNECTED) {
        stopBackgroundService()
      }
    }

    override def registerCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null) {
        callbacks.register(cb)
        callbackCount += 1
      }
    }

    override def stop() {
      if (state != State.CONNECTING && state != State.STOPPING) {
        stopRunner()
      }
    }

    override def start(config: Config) {
      if (state != State.CONNECTING && state != State.STOPPING) {
        startRunner(config)
      }
    }
  }

  def startRunner(config: Config) {
    this.config = config;

    TrafficMonitor.reset()
    trafficMonitorThread = new TrafficMonitorThread(this)
    trafficMonitorThread.start()
  }
  def stopRunner() {
    TrafficMonitor.reset()
    if (trafficMonitorThread != null) {
      trafficMonitorThread.stopThread()
      trafficMonitorThread = null
    }
  }
  def stopBackgroundService()
  def getServiceMode: Int
  def getTag: String
  def getContext: Context

  def getCallbackCount: Int = {
    callbackCount
  }
  def getState: Int = {
    state
  }
  def changeState(s: Int) {
    changeState(s, null)
  }

  def updateTraffic(txRate: String, rxRate: String, txTotal: String, rxTotal: String) {
    val handler = new Handler(getContext.getMainLooper)
    handler.post(() => {
      if (callbackCount > 0) {
        val n = callbacks.beginBroadcast()
        for (i <- 0 to n - 1) {
          try {
            callbacks.getBroadcastItem(i).trafficUpdated(txRate, rxRate, txTotal, rxTotal)
          } catch {
            case _: Exception => // Ignore
          }
        }
        callbacks.finishBroadcast()
      }
    })
  }

  protected def changeState(s: Int, msg: String) {
    val handler = new Handler(getContext.getMainLooper)
    handler.post(() => if (state != s) {
      if (callbackCount > 0) {
        val n = callbacks.beginBroadcast()
        for (i <- 0 to n - 1) {
          try {
            callbacks.getBroadcastItem(i).stateChanged(s, msg)
          } catch {
            case _: Exception => // Ignore
          }
        }
        callbacks.finishBroadcast()
      }
      state = s
    })
  }

  def initSoundVibrateLights(notification: Notification) {
    notification.sound = null
  }
}
