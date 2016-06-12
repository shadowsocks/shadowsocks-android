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

import java.util.{Timer, TimerTask}

import android.app.Service
import android.content.{BroadcastReceiver, Context, Intent, IntentFilter}
import android.os.{Handler, RemoteCallbackList}
import android.text.TextUtils
import android.util.Log
import android.widget.Toast
import com.github.shadowsocks.aidl.{Config, IShadowsocksService, IShadowsocksServiceCallback}
import com.github.shadowsocks.utils._
import com.github.shadowsocks.ShadowsocksApplication.app

trait BaseService extends Service {

  @volatile private var state = State.STOPPED
  @volatile protected var config: Config = null

  var timer: Timer = null
  var trafficMonitorThread: TrafficMonitorThread = null

  final val callbacks = new RemoteCallbackList[IShadowsocksServiceCallback]
  var callbacksCount: Int = _
  lazy val handler = new Handler(getMainLooper)

  private val closeReceiver: BroadcastReceiver = (context: Context, intent: Intent) => {
    Toast.makeText(context, R.string.stopping, Toast.LENGTH_SHORT).show()
    stopRunner(true)
  }
  var closeReceiverRegistered: Boolean = _

  val binder = new IShadowsocksService.Stub {
    override def getState: Int = {
      state
    }

    override def unregisterCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null && callbacks.unregister(cb)) {
        callbacksCount -= 1
        if (callbacksCount == 0 && timer != null) {
          timer.cancel()
          timer = null
        }
      }
    }

    override def registerCallback(cb: IShadowsocksServiceCallback) {
      if (cb != null && callbacks.register(cb)) {
        callbacksCount += 1
        if (callbacksCount != 0 && timer == null) {
          val task = new TimerTask {
            def run {
              if (TrafficMonitor.updateRate()) updateTrafficRate()
            }
          }
          timer = new Timer(true)
          timer.schedule(task, 1000, 1000)
        }
        TrafficMonitor.updateRate()
        cb.trafficUpdated(TrafficMonitor.txRate, TrafficMonitor.rxRate, TrafficMonitor.txTotal, TrafficMonitor.rxTotal)
      }
    }

    override def use(config: Config) = synchronized(state match {
      case State.STOPPED => if (config != null && checkConfig(config)) startRunner(config)
      case State.CONNECTED =>
        if (config == null) stopRunner(true)
        else if (config.profileId != BaseService.this.config.profileId && checkConfig(config)) {
          stopRunner(false)
          startRunner(config)
        }
      case _ => Log.w(BaseService.this.getClass.getSimpleName, "Illegal state when invoking use: " + state)
    })
  }

  def checkConfig(config: Config) = if (TextUtils.isEmpty(config.proxy) || TextUtils.isEmpty(config.sitekey)) {
    changeState(State.STOPPED)
    stopRunner(true)
    false
  } else true

  def startRunner(config: Config) {
    this.config = config

    startService(new Intent(this, getClass))
    TrafficMonitor.reset()
    trafficMonitorThread = new TrafficMonitorThread(getApplicationContext)
    trafficMonitorThread.start()

    if (!closeReceiverRegistered) {
      // register close receiver
      val filter = new IntentFilter()
      filter.addAction(Intent.ACTION_SHUTDOWN)
      filter.addAction(Action.CLOSE)
      registerReceiver(closeReceiver, filter)
      closeReceiverRegistered = true
    }
  }

  def stopRunner(stopService: Boolean) {
    // clean up recevier
    if (closeReceiverRegistered) {
      unregisterReceiver(closeReceiver)
      closeReceiverRegistered = false
    }

    // Make sure update total traffic when stopping the runner
    updateTrafficTotal(TrafficMonitor.txTotal, TrafficMonitor.rxTotal)

    TrafficMonitor.reset()
    if (trafficMonitorThread != null) {
      trafficMonitorThread.stopThread()
      trafficMonitorThread = null
    }

    // change the state
    changeState(State.STOPPED)

    // stop the service if nothing has bound to it
    if (stopService) stopSelf()
  }

  def updateTrafficTotal(tx: Long, rx: Long) {
    val config = this.config  // avoid race conditions without locking
    if (config != null) {
      app.profileManager.getProfile(config.profileId) match {
        case Some(profile) =>
          profile.tx += tx
          profile.rx += rx
          app.profileManager.updateProfile(profile)
        case None => // Ignore
      }
    }
  }

  def getState: Int = {
    state
  }

  def updateTrafficRate() {
    handler.post(() => {
      if (callbacksCount > 0) {
        val txRate = TrafficMonitor.txRate
        val rxRate = TrafficMonitor.rxRate
        val txTotal = TrafficMonitor.txTotal
        val rxTotal = TrafficMonitor.rxTotal
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
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

  // Service of shadowsocks should always be started explicitly
  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = Service.START_NOT_STICKY

  protected def changeState(s: Int, msg: String = null) {
    val handler = new Handler(getMainLooper)
    handler.post(() => if (state != s) {
      if (callbacksCount > 0) {
        val n = callbacks.beginBroadcast()
        for (i <- 0 until n) {
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
}
