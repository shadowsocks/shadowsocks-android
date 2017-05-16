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

package com.github.shadowsocks

import java.io._
import java.lang.System.currentTimeMillis
import java.util.concurrent.Semaphore

import android.os.Build
import android.util.Log
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils.Commandline
import com.github.shadowsocks.ShadowsocksApplication.app

import scala.collection.JavaConversions._
import scala.collection.immutable.Stream

class StreamLogger(is: InputStream, tag: String, logger: (String, String) => Int) extends Thread {
  override def run(): Unit = autoClose(new BufferedReader(new InputStreamReader(is)))(br =>
    try Stream.continually(br.readLine()).takeWhile(_ != null).foreach(logger(tag, _)) catch {
      case _: IOException =>
    })
}

/**
  * @author ayanamist@gmail.com
  */
class GuardedProcess(cmd: String*) {
  private val TAG = classOf[GuardedProcess].getSimpleName

  @volatile private var guardThread: Thread = _
  @volatile private var isDestroyed: Boolean = _
  @volatile private var process: Process = _
  @volatile private var isRestart = false

  def start(onRestartCallback: () => Unit = null): GuardedProcess = {
    val semaphore = new Semaphore(1)
    semaphore.acquire()
    @volatile var ioException: IOException = null

    guardThread = new Thread(() => {
      try {
        var callback: () => Unit = null
        while (!isDestroyed) {
          if (BuildConfig.DEBUG) Log.d(TAG, "start process: " + Commandline.toString(cmd))
          val startTime = currentTimeMillis

          process = new ProcessBuilder(cmd)
            .redirectErrorStream(true)
            .directory(app.getFilesDir)
            .start()

          new StreamLogger(process.getInputStream(), TAG, Log.i).start()
          new StreamLogger(process.getErrorStream(), TAG, Log.e).start()

          if (callback == null) callback = onRestartCallback else callback()

          semaphore.release()
          process.waitFor

          this.synchronized {
            if (isRestart) {
              isRestart = false
            } else {
              if (currentTimeMillis - startTime < 1000) {
                Log.w(TAG, "process exit too fast, stop guard: " + Commandline.toString(cmd))
                isDestroyed = true
              }
            }
          }
        }
      } catch {
        case _: InterruptedException =>
          if (BuildConfig.DEBUG) Log.d(TAG, "thread interrupt, destroy process: " + Commandline.toString(cmd))
          destroyProcess()
        case e: IOException => ioException = e
      } finally semaphore.release()
    }, "GuardThread-" + cmd.head)

    guardThread.start()
    semaphore.acquire()

    if (ioException != null) throw ioException

    this
  }

  def destroy() {
    isDestroyed = true
    guardThread.interrupt()
    destroyProcess()
    try guardThread.join() catch {
      case _: InterruptedException =>
    }
  }

  private def destroyProcess() {
    if (Build.VERSION.SDK_INT < 24) {
      JniHelper.sigtermCompat(process)
      JniHelper.waitForCompat(process, 500)
    }
    process.destroy()
  }

  def restart() {
    this.synchronized {
      isRestart = true
      destroyProcess()
    }
  }

  @throws(classOf[InterruptedException])
  def waitFor: Int = {
    guardThread.join()
    0
  }
}
