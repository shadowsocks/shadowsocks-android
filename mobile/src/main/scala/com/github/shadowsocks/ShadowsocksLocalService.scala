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

import java.io.File
import java.net.{Inet6Address, InetAddress}
import java.util.Locale

import android.app.Service
import android.content._
import android.os._
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.{Acl, AclSyncJob}
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils._

import scala.collection.JavaConversions._
import scala.collection.mutable.ArrayBuffer

class ShadowsocksLocalService extends BaseService {

  val TAG = "ShadowsocksLocalService"

  var sslocalProcess: GuardedProcess = _

  def startShadowsocksDaemon() {
    val cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libss-local.so",
      "-b", "127.0.0.1",
      "-l", profile.localPort.toString,
      "-t", "600",
      "-c", buildShadowsocksConfig("ss-local-local.conf"))

    if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

    if (profile.route != Acl.ALL) {
      cmd += "--acl"
      cmd += Acl.getFile(profile.route match {
        case Acl.CUSTOM_RULES => Acl.CUSTOM_RULES_FLATTENED
        case route => route
      }).getAbsolutePath
    }

    sslocalProcess = new GuardedProcess(cmd: _*).start()
  }

  /** Called when the activity is first created. */
  def handleConnection() {

    startShadowsocksDaemon()

  }

  def onBind(intent: Intent): IBinder = {
    Log.d(TAG, "onBind")
    if (Action.SERVICE == intent.getAction) {
      binder
    } else {
      null
    }
  }

  def killProcesses() {
    if (sslocalProcess != null) {
      sslocalProcess.destroy()
      sslocalProcess = null
    }
  }

  override def connect() {
    super.connect()

    // Clean up
    killProcesses()

    if (!Utils.isNumeric(profile.host)) Utils.resolve(profile.host, enableIPv6 = true) match {
      case Some(a) => profile.host = a
      case None => throw NameNotResolvedException()
    }

    handleConnection()

    if (profile.route != Acl.ALL && profile.route != Acl.CUSTOM_RULES)
      AclSyncJob.schedule(profile.route)

    changeState(State.CONNECTED)
  }

  override def createNotification() = new ShadowsocksNotification(this, profile.name, "service-local", true)

  override def stopRunner(stopService: Boolean, msg: String = null) {

    // channge the state
    changeState(State.STOPPING)

    app.track(TAG, "stop")

    // reset NAT
    killProcesses()

    super.stopRunner(stopService, msg)
  }
}
