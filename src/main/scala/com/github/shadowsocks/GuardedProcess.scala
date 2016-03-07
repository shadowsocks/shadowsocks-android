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

import android.util.Log
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicReference

import collection.JavaConversions._

/**
  * @author ayanamist@gmail.com
  */
class GuardedProcess (cmd: Seq[String], onRestartCallback: Runnable = null) extends Process {

  private val TAG = classOf[GuardedProcess].getSimpleName

  @volatile private var guardThread: Thread = null
  @volatile private var isDestroyed: Boolean = false
  @volatile private var process: Process = null

  def start(): GuardedProcess = {

    val atomicIoException = new AtomicReference[IOException](null)
    val countDownLatch = new CountDownLatch(1)

    guardThread = new Thread(new Runnable() {
      override def run() {
        try {
          while (!isDestroyed) {
            Log.i(TAG, "start process: " + cmd)
            val startTime = java.lang.System.currentTimeMillis

            process = new ProcessBuilder(cmd).redirectErrorStream(true).start
            if (onRestartCallback != null && countDownLatch.getCount <= 0) {
              onRestartCallback.run()
            }

            countDownLatch.countDown()
            process.waitFor

            if (java.lang.System.currentTimeMillis - startTime < 1000) {
              Log.w(TAG, "process exit too fast, stop guard: " + cmd)
              return
            }
          }
        } catch {
          case ignored: InterruptedException =>
            Log.i(TAG, "thread interrupt, destroy process: " + cmd)
            process.destroy()
          case e: IOException =>
            atomicIoException.compareAndSet(null, e)
        } finally {
          countDownLatch.countDown()
        }
      }
    }, "GuardThread-" + cmd)

    guardThread.start()
    countDownLatch.await()

    val ioException: IOException = atomicIoException.get
    if (ioException != null) {
      throw ioException
    }

    this
  }

  def destroy() {
    isDestroyed = true
    guardThread.interrupt()
    process.destroy()
    try {
      guardThread.join()
    }
    catch {
      case ignored: InterruptedException =>
    }
  }

  def exitValue: Int = {
    throw new UnsupportedOperationException
  }

  def getErrorStream: InputStream = {
    throw new UnsupportedOperationException
  }

  def getInputStream: InputStream = {
    throw new UnsupportedOperationException
  }

  def getOutputStream: OutputStream = {
    throw new UnsupportedOperationException
  }

  @throws(classOf[InterruptedException])
  def waitFor: Int = {
    guardThread.join()
    0
  }
}
