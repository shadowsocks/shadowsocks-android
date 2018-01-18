/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.bg

import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.net.LocalSocket
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.JniHelper
import com.github.shadowsocks.R
import com.github.shadowsocks.VpnRequestActivity
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Subnet
import com.github.shadowsocks.utils.parseNumericAddress
import java.io.File
import java.io.FileDescriptor
import java.lang.reflect.Method
import java.util.*
import android.net.VpnService as BaseVpnService

class VpnService : BaseVpnService(), LocalDnsService.Interface {
    companion object {
        private const val VPN_MTU = 1500
        private const val PRIVATE_VLAN = "26.26.26.%s"
        private const val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"

        private val getInt: Method = FileDescriptor::class.java.getDeclaredMethod("getInt$")
    }

    private inner class ProtectWorker : LocalSocketListener("ShadowsocksVpnThread") {
        override val socketFile: File = File(app.deviceContext.filesDir, "protect_path")

        override fun accept(socket: LocalSocket) {
            try {
                socket.inputStream.read()
                val fds = socket.ancillaryFileDescriptors
                if (fds.isEmpty()) return
                val fd = getInt.invoke(fds.first()) as Int
                val ret = protect(fd)
                JniHelper.close(fd) // Trick to close file decriptor
                socket.outputStream.write(if (ret) 0 else 1)
            } catch (e: Exception) {
                Log.e(tag, "Error when protect socket", e)
                app.track(e)
            }
        }
    }
    class NullConnectionException : NullPointerException()

    init {
        BaseService.register(this)
    }

    override val tag: String get() = "ShadowsocksVpnService"
    override fun createNotification(profileName: String): ServiceNotification =
            ServiceNotification(this, profileName, "service-vpn")

    private var conn: ParcelFileDescriptor? = null
    private var worker: ProtectWorker? = null
    private var tun2socksProcess: GuardedProcess? = null

    override fun onBind(intent: Intent): IBinder? = when (intent.action) {
        SERVICE_INTERFACE -> super<BaseVpnService>.onBind(intent)
        else -> super<LocalDnsService.Interface>.onBind(intent)
    }

    override fun onRevoke() = stopRunner(true)

    override fun killProcesses() {
        worker?.stopThread()
        worker = null
        super.killProcesses()
        tun2socksProcess?.destroy()
        tun2socksProcess = null
        conn?.close()
        conn = null
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (BaseService.usingVpnMode)
            if (BaseVpnService.prepare(this) != null)
                startActivity(Intent(this, VpnRequestActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            else return super<LocalDnsService.Interface>.onStartCommand(intent, flags, startId)
        stopRunner(true)
        return Service.START_NOT_STICKY
    }

    override fun startNativeProcesses() {
        val worker = ProtectWorker()
        worker.start()
        this.worker = worker

        super.startNativeProcesses()

        val fd = startVpn()
        if (!sendFd(fd)) throw Exception("sendFd failed")
    }

    override fun buildAdditionalArguments(cmd: ArrayList<String>): ArrayList<String> {
        cmd += "-V"
        return cmd
    }

    private fun startVpn(): Int {
        val profile = data.profile!!
        val builder = Builder()
                .setSession(profile.formattedName)
                .setMtu(VPN_MTU)
                .addAddress(PRIVATE_VLAN.format(Locale.ENGLISH, "1"), 24)

        profile.remoteDns.split(",").forEach { builder.addDnsServer(it.trim()) }

        if (profile.ipv6) {
            builder.addAddress(PRIVATE_VLAN6.format(Locale.ENGLISH, "1"), 126)
            builder.addRoute("::", 0)
        }

        if (profile.proxyApps) {
            val me = packageName
            profile.individual.split('\n')
                    .filter { it != me }
                    .forEach {
                        try {
                            if (profile.bypass) builder.addDisallowedApplication(it)
                            else builder.addAllowedApplication(it)
                        } catch (ex: PackageManager.NameNotFoundException) {
                            Log.e(tag, "Invalid package name", ex)
                        }
                    }
            if (!profile.bypass) builder.addAllowedApplication(me)
        }

        when (profile.route) {
            Acl.ALL, Acl.BYPASS_CHN, Acl.CUSTOM_RULES -> builder.addRoute("0.0.0.0", 0)
            else -> {
                resources.getStringArray(R.array.bypass_private_route).forEach {
                    val subnet = Subnet.fromString(it)!!
                    builder.addRoute(subnet.address.hostAddress, subnet.prefixSize)
                }
                profile.remoteDns.split(",").mapNotNull { it.trim().parseNumericAddress() }
                        .forEach { builder.addRoute(it, it.address.size shl 3) }
            }
        }

        val conn = builder.establish() ?: throw NullConnectionException()
        this.conn = conn
        val fd = conn.fd

        val cmd = arrayListOf(File(applicationInfo.nativeLibraryDir, Executable.TUN2SOCKS).absolutePath,
                "--netif-ipaddr", PRIVATE_VLAN.format(Locale.ENGLISH, "2"),
                "--netif-netmask", "255.255.255.0",
                "--socks-server-addr", "127.0.0.1:${DataStore.portProxy}",
                "--tunfd", fd.toString(),
                "--tunmtu", VPN_MTU.toString(),
                "--sock-path", "sock_path",
                "--loglevel", "3")
        if (profile.ipv6) {
            cmd += "--netif-ip6addr"
            cmd += PRIVATE_VLAN6.format(Locale.ENGLISH, "2")
        }
        cmd += "--enable-udprelay"
        if (!profile.udpdns) {
            cmd += "--dnsgw"
            cmd += "127.0.0.1:${DataStore.portLocalDns}"
        }
        tun2socksProcess = GuardedProcess(cmd).start { sendFd(fd) }
        return fd
    }

    private fun sendFd(fd: Int): Boolean {
        if (fd != -1) {
            var tries = 0
            while (tries < 10) {
                Thread.sleep(30L shl tries)
                if (JniHelper.sendFd(fd, File(app.deviceContext.filesDir, "sock_path").absolutePath) != -1) return true
                tries += 1
            }
        }
        return false
    }
}
