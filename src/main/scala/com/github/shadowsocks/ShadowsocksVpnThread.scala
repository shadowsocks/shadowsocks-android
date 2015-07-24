/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2015 <max.c.lv@gmail.com>
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

import java.io.{File, FileDescriptor, IOException}

import android.net.{LocalServerSocket, LocalSocket, LocalSocketAddress}
import android.util.Log

class ShadowsocksVpnThread(vpnService: ShadowsocksVpnService) extends Runnable {

  val TAG = "ShadowsocksVpnService"
  val PATH = "/data/data/com.github.shadowsocks/protect_path"

  override def run(): Unit = {

    var serverSocket: LocalServerSocket = null

    try {
      new File(PATH).delete()
    } catch {
      case _ => // ignore
    }

    try {
      val localSocket = new LocalSocket

      localSocket.bind(new LocalSocketAddress(PATH, LocalSocketAddress.Namespace.FILESYSTEM))
      serverSocket = new LocalServerSocket(localSocket.getFileDescriptor)
    } catch {
      case e: IOException =>
        Log.e(TAG, "unable to bind", e)
        vpnService.stopBackgroundService()
        return
    }

    Log.d(TAG, "start to accept connections")

    while (true) {

      try {
        val socket = serverSocket.accept()

        val input = socket.getInputStream
        val output = socket.getOutputStream

        input.read()

        Log.d(TAG, "test")

        val fds = socket.getAncillaryFileDescriptors

        if (fds.length > 0) {
          var ret = false

          val getInt = classOf[FileDescriptor].getDeclaredMethod("getInt$")
          val fd = getInt.invoke(fds(0)).asInstanceOf[Int]
          ret = vpnService.protect(fd)

          if (ret) {
            socket.setFileDescriptorsForSend(fds)
            output.write(0)
          }

          input.close()
          output.close()
        }
      } catch {
        case e: Exception =>
          Log.e(TAG, "Error when protect socket", e)
      }

    }
  }
}
