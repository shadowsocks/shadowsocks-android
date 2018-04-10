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

import android.annotation.TargetApi
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.*
import android.os.Build
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.support.v4.os.BuildCompat
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.JniHelper
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.VpnRequestActivity
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Subnet
import com.github.shadowsocks.utils.parseNumericAddress
import java.io.File
import java.io.FileDescriptor
import java.io.IOException
import java.lang.reflect.Method
import java.util.*
import android.net.VpnService as BaseVpnService

class VpnService : BaseVpnService(), LocalDnsService.Interface {
    companion object {
        private const val VPN_MTU = 1500
        private const val PRIVATE_VLAN = "26.26.26.%s"
        private const val PRIVATE_VLAN6 = "fdfe:dcba:9876::%s"

        private val getInt: Method = FileDescriptor::class.java.getDeclaredMethod("getInt$")

        /**
         * Unfortunately registerDefaultNetworkCallback is going to return VPN interface since Android P DP1:
         * https://android.googlesource.com/platform/frameworks/base/+/dda156ab0c5d66ad82bdcf76cda07cbc0a9c8a2e
         *
         * This makes doing a requestNetwork with REQUEST necessary so that we don't get ALL possible networks that
         * satisfies default network capabilities but only THE default network. Unfortunately we need to have
         * android.permission.CHANGE_NETWORK_STATE to be able to call requestNetwork.
         *
         * Source: https://android.googlesource.com/platform/frameworks/base/+/2df4c7d/services/core/java/com/android/server/ConnectivityService.java#887
         */
        private val defaultNetworkRequest = NetworkRequest.Builder()
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)
                .build()
    }

    private inner class ProtectWorker : LocalSocketListener("ShadowsocksVpnThread") {
        override val socketFile: File = File(app.deviceContext.filesDir, "protect_path")

        override fun accept(socket: LocalSocket) {
            var success = false
            try {
                socket.inputStream.read()
                val fd = socket.ancillaryFileDescriptors!!.single()!!
                val fdInt = getInt.invoke(fd) as Int
                try {
                    val network = underlyingNetwork
                    success = if (network != null && Build.VERSION.SDK_INT >= 23) {
                        network.bindSocket(fd)
                        true
                    } else protect(fdInt)
                } catch (e: Exception) {
                    Log.e(tag, "Error when protect socket", e)
                    app.track(e)
                } finally {
                    JniHelper.close(fdInt) // Trick to close file decriptor
                }
            } catch (e: Exception) {
                Log.e(tag, "Error when receiving ancillary fd", e)
                app.track(e)
            }
            try {
                socket.outputStream.write(if (success) 0 else 1)
            } catch (e: IOException) {
                Log.e(tag, "Error when returning result in protect", e)
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
    private var underlyingNetwork: Network? = null
        @TargetApi(28)
        set(value) {
            setUnderlyingNetworks(if (value == null) null else arrayOf(value))
            field = value
        }

    private val connectivity by lazy { getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager }
    @TargetApi(28)
    private val defaultNetworkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onAvailable(network: Network) {
            underlyingNetwork = network
        }
        override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities?) {
            // it's a good idea to refresh capabilities
            underlyingNetwork = network
        }
        override fun onLost(network: Network) {
            underlyingNetwork = null
        }
    }
    private var listeningForDefaultNetwork = false

    override fun onBind(intent: Intent): IBinder? = when (intent.action) {
        SERVICE_INTERFACE -> super<BaseVpnService>.onBind(intent)
        else -> super<LocalDnsService.Interface>.onBind(intent)
    }

    override fun onRevoke() = stopRunner(true)

    override fun killProcesses() {
        if (listeningForDefaultNetwork) {
            connectivity.unregisterNetworkCallback(defaultNetworkCallback)
            listeningForDefaultNetwork = false
        }
        worker?.stopThread()
        worker = null
        super.killProcesses()
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
        if (!sendFd(fd)) throw IOException("sendFd failed")
    }

    override fun buildAdditionalArguments(cmd: ArrayList<String>): ArrayList<String> {
        cmd += "-V"
        return cmd
    }

    private fun startVpn(): Int {
        val profile = data.profile!!
        val builder = Builder()
                .setConfigureIntent(MainActivity.pendingIntent(this))
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

        if (BuildCompat.isAtLeastP()) {
            // we want REQUEST here instead of LISTEN
            connectivity.requestNetwork(defaultNetworkRequest, defaultNetworkCallback)
            listeningForDefaultNetwork = true
        }

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
        data.processes.start(cmd) { sendFd(fd) }
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
