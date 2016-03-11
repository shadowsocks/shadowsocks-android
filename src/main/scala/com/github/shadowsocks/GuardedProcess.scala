/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2016 <max.c.lv@gmail.com>
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

import java.io.{IOException, InputStream, OutputStream}
import java.lang.System.currentTimeMillis
import java.util.concurrent.Semaphore

import android.util.Log

import scala.collection.JavaConversions._

/**
  * @author ayanamist@gmail.com
  */
class GuardedProcess(cmd: Seq[String]) extends Process {
  private val TAG = classOf[GuardedProcess].getSimpleName

  @volatile private var guardThread: Thread = _
  @volatile private var isDestroyed: Boolean = _
  @volatile private var process: Process = _

  def start(onRestartCallback: () => Unit = null): GuardedProcess = {
    val semaphore = new Semaphore(1)
    semaphore.acquire
    @volatile var ioException: IOException = null

    guardThread = new Thread(() => {
      try {
        var callback: () => Unit = null
        while (!isDestroyed) {
          Log.i(TAG, "start process: " + cmd)
          val startTime = currentTimeMillis

          process = new ProcessBuilder(cmd).redirectErrorStream(true).start

          if (callback == null) callback = onRestartCallback else callback()

          semaphore.release
          process.waitFor

          if (currentTimeMillis - startTime < 1000) {
            Log.w(TAG, "process exit too fast, stop guard: " + cmd)
            isDestroyed = true
          }
        }
      } catch {
        case ignored: InterruptedException =>
          Log.i(TAG, "thread interrupt, destroy process: " + cmd)
          process.destroy()
        case e: IOException => ioException = e
      } finally semaphore.release
    }, "GuardThread-" + cmd)

    guardThread.start()
    semaphore.acquire

    if (ioException != null) {
      throw ioException
    }

    this
  }

  def destroy() {
    isDestroyed = true
    guardThread.interrupt()
    process.destroy()
    try guardThread.join() catch {
      case ignored: InterruptedException =>
    }
  }

  def exitValue: Int = throw new UnsupportedOperationException
  def getErrorStream: InputStream = throw new UnsupportedOperationException
  def getInputStream: InputStream = throw new UnsupportedOperationException
  def getOutputStream: OutputStream = throw new UnsupportedOperationException

  @throws(classOf[InterruptedException])
  def waitFor = {
    guardThread.join()
    0
  }
}
