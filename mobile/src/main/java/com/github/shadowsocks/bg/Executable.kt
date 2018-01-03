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

import android.text.TextUtils
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.JniHelper
import java.io.File

object Executable {
    const val REDSOCKS = "libredsocks.so"
    const val SS_LOCAL = "libss-local.so"
    const val SS_TUNNEL = "libss-tunnel.so"
    const val TUN2SOCKS = "libtun2socks.so"
    const val OVERTURE = "liboverture.so"

    val EXECUTABLES = setOf(SS_LOCAL, SS_TUNNEL, REDSOCKS, TUN2SOCKS, OVERTURE)

    fun killAll() {
        for (process in File("/proc").listFiles { _, name -> TextUtils.isDigitsOnly(name) }) {
            val exe = File(File(process, "cmdline").readText().split(Character.MIN_VALUE, limit = 2).first())
            if (exe.parent == app.applicationInfo.nativeLibraryDir && EXECUTABLES.contains(exe.name)) {
                val errno = JniHelper.sigkill(process.name.toInt())
                if (errno != 0) Log.w("kill", "SIGKILL ${exe.absolutePath} (${process.name}) failed with $errno")
            }
        }
    }
}
