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
import java.util.Locale

import android.app.Service
import android.content._
import android.content.pm.PackageManager.NameNotFoundException
import android.net.VpnService
import android.os._
import android.util.Log
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.{Acl, AclSyncJob, Subnet}
import com.github.shadowsocks.utils._

import scala.collection.mutable.ArrayBuffer

class ShadowsocksVpnService extends VpnService with BaseService {
  val TAG = "ShadowsocksVpnService"
  val VPN_MTU = 1500
  val PRIVATE_VLAN = "26.26.26.%s"
  val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"
  var conn: ParcelFileDescriptor = _
  var vpnThread: ShadowsocksVpnThread = _

  var sslocalProcess: GuardedProcess = _
  var overtureProcess: GuardedProcess = _
  var tun2socksProcess: GuardedProcess = _

  override def onBind(intent: Intent): IBinder = {
    val action = intent.getAction
    if (VpnService.SERVICE_INTERFACE == action) {
      return super.onBind(intent)
    } else if (Action.SERVICE == action) {
      return binder
    }
    null
  }

  override def onRevoke() {
    stopRunner(stopService = true)
  }

  override def stopRunner(stopService: Boolean, msg: String = null) {

    if (vpnThread != null) {
      vpnThread.stopThread()
      vpnThread = null
    }

    // channge the state
    changeState(State.STOPPING)

    app.track(TAG, "stop")

    // reset VPN
    killProcesses()

    // close connections
    if (conn != null) {
      conn.close()
      conn = null
    }

    super.stopRunner(stopService, msg)
  }

  def killProcesses() {
    if (sslocalProcess != null) {
      sslocalProcess.destroy()
      sslocalProcess = null
    }
    if (tun2socksProcess != null) {
      tun2socksProcess.destroy()
      tun2socksProcess = null
    }
    if (overtureProcess != null) {
      overtureProcess.destroy()
      overtureProcess = null
    }
  }

  override def onStartCommand(intent: Intent, flags: Int, startId: Int): Int = {
    // ensure the VPNService is prepared
    if (VpnService.prepare(this) != null) {
      val i = new Intent(this, classOf[ShadowsocksRunnerActivity])
      i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
      startActivity(i)
      stopRunner(stopService = true)
      Service.START_NOT_STICKY
    } else super.onStartCommand(intent, flags, startId)
  }

  override def createNotification() = new ShadowsocksNotification(this, profile.name, "service-vpn")

  override def connect() {
    super.connect()

    vpnThread = new ShadowsocksVpnThread(this)
    vpnThread.start()

    // reset the context
    killProcesses()

    // Resolve the server address
    if (!Utils.isNumeric(profile.host)) Utils.resolve(profile.host, enableIPv6 = true) match {
      case Some(addr) => profile.host = addr
      case None => throw NameNotResolvedException()
    }

    handleConnection()
    changeState(State.CONNECTED)

    if (profile.route != Acl.ALL && profile.route != Acl.CUSTOM_RULES)
      AclSyncJob.schedule(profile.route)
  }

  /** Called when the activity is first created. */
  def handleConnection() {

    startShadowsocksDaemon()

    if (!profile.udpdns) {
      startDnsDaemon()
    }

    val fd = startVpn()
    if (!sendFd(fd)) throw new Exception("sendFd failed")
  }

  override protected def buildPluginCommandLine(): ArrayBuffer[String] = super.buildPluginCommandLine() += "-V"

  def startShadowsocksDaemon() {
    val cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libss-local.so",
      "-V",
      "-u",
      "-b", "127.0.0.1",
      "-l", profile.localPort.toString,
      "-t", "600",
      "-c", buildShadowsocksConfig("ss-local-vpn.conf"))

    if (profile.route != Acl.ALL) {
      cmd += "--acl"
      cmd += Acl.getFile(profile.route).getAbsolutePath
    }

    if (TcpFastOpen.sendEnabled) cmd += "--fast-open"

    sslocalProcess = new GuardedProcess(cmd: _*).start()
  }

  def startDnsDaemon() {
    overtureProcess = new GuardedProcess(getApplicationInfo.nativeLibraryDir + "/liboverture.so",
      "-c", buildOvertureConfig("overture-vpn.conf"), "-V")
      .start()
  }

  def startVpn(): Int = {

    val builder = new Builder()
    builder
      .setSession(profile.getName)
      .setMtu(VPN_MTU)
      .addAddress(PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "1"), 24)

    builder.addDnsServer("8.8.8.8") // It's fake DNS for tun2socks, not the real remote DNS

    if (profile.ipv6) {
      builder.addAddress(PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "1"), 126)
      builder.addRoute("::", 0)
    }

    if (Utils.isLollipopOrAbove) {

      if (profile.proxyApps) {
        for (pkg <- profile.individual.split('\n')) {
          try {
            if (!profile.bypass) {
              builder.addAllowedApplication(pkg)
            } else {
              builder.addDisallowedApplication(pkg)
            }
          } catch {
            case ex: NameNotFoundException =>
              Log.e(TAG, "Invalid package name", ex)
          }
        }
      }
    }

    if (profile.route == Acl.ALL
      || profile.route == Acl.BYPASS_CHN
      || profile.route == Acl.CUSTOM_RULES) {
      builder.addRoute("0.0.0.0", 0)
    } else {
      getResources.getStringArray(R.array.bypass_private_route).foreach(cidr => {
        val subnet = Subnet.fromString(cidr)
        builder.addRoute(subnet.address.getHostAddress, subnet.prefixSize)
      })
      profile.remoteDns.split(",").map(dns => Utils.parseNumericAddress(dns.trim)).foreach(dns =>
        builder.addRoute(dns, dns.getAddress.length << 3))
    }

    conn = builder.establish()
    if (conn == null) throw new NullConnectionException

    val fd = conn.getFd

    var cmd = ArrayBuffer[String](getApplicationInfo.nativeLibraryDir + "/libtun2socks.so",
      "--netif-ipaddr", PRIVATE_VLAN.formatLocal(Locale.ENGLISH, "2"),
      "--netif-netmask", "255.255.255.0",
      "--socks-server-addr", "127.0.0.1:" + profile.localPort,
      "--tunfd", fd.toString,
      "--tunmtu", VPN_MTU.toString,
      "--sock-path", "sock_path",
      "--loglevel", "3")

    if (profile.ipv6)
      cmd += ("--netif-ip6addr", PRIVATE_VLAN6.formatLocal(Locale.ENGLISH, "2"))

    cmd += "--enable-udprelay"

    if (!profile.udpdns)
      cmd += ("--dnsgw", "%s:%d".formatLocal(Locale.ENGLISH, "127.0.0.1",
        profile.localPort + 53))

    tun2socksProcess = new GuardedProcess(cmd: _*).start(() => sendFd(fd))

    fd
  }

  def sendFd(fd: Int): Boolean = {
    if (fd != -1) {
      var tries = 1
      while (tries < 5) {
        Thread.sleep(1000 * tries)
        if (JniHelper.sendFd(fd, new File(getFilesDir, "sock_path").getAbsolutePath) != -1) {
          return true
        }
        tries += 1
      }
    }
    false
  }
}
