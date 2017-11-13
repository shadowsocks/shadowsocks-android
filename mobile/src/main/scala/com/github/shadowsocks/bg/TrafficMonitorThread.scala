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

import java.io.{File, IOException}
import java.nio.{ByteBuffer, ByteOrder}
import java.util.concurrent.Executors

import android.content.Context
import android.net.{LocalServerSocket, LocalSocket, LocalSocketAddress}
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app

class TrafficMonitorThread(context: Context) extends Thread {

  val TAG = "TrafficMonitorThread"
  val stat = new File(context.getFilesDir, "/stat_path")

  @volatile var serverSocket: LocalServerSocket = _
  @volatile var isRunning: Boolean = true

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

    stat.delete()

    try {
      val localSocket = new LocalSocket
      localSocket.bind(new LocalSocketAddress(stat.getAbsolutePath, LocalSocketAddress.Namespace.FILESYSTEM))
      serverSocket = new LocalServerSocket(localSocket.getFileDescriptor)
    } catch {
      case e: IOException =>
        Log.e(TAG, "unable to bind", e)
        return
    }

    val pool = Executors.newFixedThreadPool(1)

    while (isRunning) {
      try {
        val socket = serverSocket.accept()

        pool.execute(() => {
          try {
            val input = socket.getInputStream
            val output = socket.getOutputStream

            val buffer = new Array[Byte](16)
            if (input.read(buffer) != 16) throw new IOException("Unexpected traffic stat length")
            val stat = ByteBuffer.wrap(buffer).order(ByteOrder.LITTLE_ENDIAN)
            TrafficMonitor.update(stat.getLong(0), stat.getLong(8))

            output.write(0)

            input.close()
            output.close()

          } catch {
            case e: Exception =>
              Log.e(TAG, "Error when recv traffic stat", e)
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
