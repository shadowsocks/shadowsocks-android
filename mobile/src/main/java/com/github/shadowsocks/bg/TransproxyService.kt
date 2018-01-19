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
import android.os.IBinder
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.preference.DataStore
import java.io.File

class TransproxyService : Service(), LocalDnsService.Interface {
    init {
        BaseService.register(this)
    }

    override val tag: String get() = "ShadowsocksTransproxyService"
    override fun createNotification(profileName: String): ServiceNotification =
            ServiceNotification(this, profileName, "service-transproxy", true)

    override fun onBind(intent: Intent): IBinder? = super.onBind(intent)
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int =
            super<LocalDnsService.Interface>.onStartCommand(intent, flags, startId)

    private var sstunnelProcess: GuardedProcess? = null
    private var redsocksProcess: GuardedProcess? = null

    private fun startDNSTunnel() {
        val cmd = arrayListOf(File(applicationInfo.nativeLibraryDir, Executable.SS_TUNNEL).absolutePath,
                "-t", "10",
                "-b", "127.0.0.1",
                "-u",
                "-l", DataStore.portLocalDns.toString(),            // ss-tunnel listens on the same port as overture
                "-L", data.profile!!.remoteDns.split(",").first().trim() + ":53",
                "-c", data.shadowsocksConfigFile!!.absolutePath)    // config is already built by BaseService.Interface
        sstunnelProcess = GuardedProcess(cmd).start()
    }

    private fun startRedsocksDaemon() {
        File(app.deviceContext.filesDir, "redsocks.conf").writeText("""base {
 log_debug = off;
 log_info = off;
 log = stderr;
 daemon = off;
 redirector = iptables;
}
redsocks {
 local_ip = 127.0.0.1;
 local_port = ${DataStore.portTransproxy};
 ip = 127.0.0.1;
 port = ${DataStore.portProxy};
 type = socks5;
}
""")
        redsocksProcess = GuardedProcess(arrayListOf(
                File(applicationInfo.nativeLibraryDir, Executable.REDSOCKS).absolutePath,
                "-c", "redsocks.conf")
        ).start()
    }

    override fun startNativeProcesses() {
        startRedsocksDaemon()
        super.startNativeProcesses()
        if (data.profile!!.udpdns) startDNSTunnel()
    }

    override fun killProcesses() {
        super.killProcesses()
        sstunnelProcess?.destroy()
        sstunnelProcess = null
        redsocksProcess?.destroy()
        redsocksProcess = null
    }
}
