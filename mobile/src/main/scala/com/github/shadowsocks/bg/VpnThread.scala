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

import java.io.{File, FileDescriptor, IOException}
import java.lang.reflect.Method
import java.util.concurrent.Executors

import android.net.{LocalServerSocket, LocalSocket, LocalSocketAddress}
import android.util.Log
import com.github.shadowsocks.JniHelper
import com.github.shadowsocks.ShadowsocksApplication.app

object VpnThread {
  val getInt: Method = classOf[FileDescriptor].getDeclaredMethod("getInt$")
}

class VpnThread(service: VpnService) extends Thread {
  import VpnThread._

  val TAG = "ShadowsocksVpnService"
  val protect = new File(service.getFilesDir, "protect_path")

  @volatile var isRunning: Boolean = true
  @volatile var serverSocket: LocalServerSocket = _

  def closeServerSocket() {
    if (serverSocket != null) {
      try {
        serverSocket.close()
      } catch {
        case _: Exception => // ignore
      }
      serverSocket = null
    }
  }

  def stopThread() {
    isRunning = false
    closeServerSocket()
  }

  override def run() {

    protect.delete()

    try {
      val localSocket = new LocalSocket
      localSocket.bind(new LocalSocketAddress(protect.getAbsolutePath, LocalSocketAddress.Namespace.FILESYSTEM))
      serverSocket = new LocalServerSocket(localSocket.getFileDescriptor)
    } catch {
      case e: IOException =>
        Log.e(TAG, "unable to bind", e)
        app.track(e)
        return
    }

    val pool = Executors.newFixedThreadPool(4)

    while (isRunning) {
      try {
        val socket = serverSocket.accept()

        pool.execute(() => {
          try {
            val input = socket.getInputStream
            val output = socket.getOutputStream

            input.read()

            val fds = socket.getAncillaryFileDescriptors

            if (fds.nonEmpty) {
              val fd = getInt.invoke(fds(0)).asInstanceOf[Int]
              val ret = service.protect(fd)

              // Trick to close file decriptor
              JniHelper.close(fd)

              if (ret) {
                output.write(0)
              } else {
                output.write(1)
              }
            }

            input.close()
            output.close()

          } catch {
            case e: Exception =>
              Log.e(TAG, "Error when protect socket", e)
              app.track(e)
          }

          // close socket
          try {
            socket.close()
          } catch {
            case _: Exception => // ignore
          }

        })
      } catch {
        case e: IOException =>
          Log.e(TAG, "Error when accept socket", e)
          app.track(e)
          return
      }
    }
  }
}
