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

import java.io.File
import java.util.Locale

import com.github.shadowsocks.GuardedProcess
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.IOUtils

import scala.collection.mutable.ArrayBuffer

object TransproxyService {
  private val REDSOCKS_CONFIG = "base {\n" +
    " log_debug = off;\n" +
    " log_info = off;\n" +
    " log = stderr;\n" +
    " daemon = off;\n" +
    " redirector = iptables;\n" +
    "}\n" +
    "redsocks {\n" +
    " local_ip = 127.0.0.1;\n" +
    " local_port = %d;\n" +
    " ip = 127.0.0.1;\n" +
    " port = %d;\n" +
    " type = socks5;\n" +
    "}\n"
}

class TransproxyService extends LocalDnsService {
  import TransproxyService._

  val TAG = "ShadowsocksTransproxyService"

  var sstunnelProcess: GuardedProcess = _
  var redsocksProcess: GuardedProcess = _

  def startDNSTunnel() {
    val cmd = ArrayBuffer[String](new File(getApplicationInfo.nativeLibraryDir, Executable.SS_TUNNEL).getAbsolutePath,
      "-t", "10",
      "-b", "127.0.0.1",
      "-u",
      "-l", app.dataStore.portLocalDns.toString,  // ss-tunnel listens on the same port as overture
      "-L", profile.remoteDns.split(",").head.trim + ":53",
      "-c", "shadowsocks.json") // config is already built by BaseService

    sstunnelProcess = new GuardedProcess(cmd: _*).start()
  }

  def startRedsocksDaemon() {
    IOUtils.writeString(new File(getFilesDir, "redsocks.conf"),
      REDSOCKS_CONFIG.formatLocal(Locale.ENGLISH, app.dataStore.portTransproxy, app.dataStore.portProxy))
    redsocksProcess = new GuardedProcess(
      new File(getApplicationInfo.nativeLibraryDir, Executable.REDSOCKS).getAbsolutePath,
      "-c", "redsocks.conf"
    ).start()
  }

  override def startNativeProcesses() {
    startRedsocksDaemon()
    super.startNativeProcesses()
    if (profile.udpdns) startDNSTunnel()
  }

  override def killProcesses() {
    super.killProcesses()
    if (sstunnelProcess != null) {
      sstunnelProcess.destroy()
      sstunnelProcess = null
    }
    if (redsocksProcess != null) {
      redsocksProcess.destroy()
      redsocksProcess = null
    }
  }

  def createNotification() = new ServiceNotification(this, profile.name, "service-transproxy", true)
}
