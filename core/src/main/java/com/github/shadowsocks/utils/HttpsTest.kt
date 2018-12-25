/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2018 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2018 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

package com.github.shadowsocks.utils

import android.os.SystemClock
import com.github.shadowsocks.Core
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.core.R
import com.github.shadowsocks.preference.DataStore
import java.io.IOException
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Proxy
import java.net.URL

/**
 * Based on: https://android.googlesource.com/platform/frameworks/base/+/b19a838/services/core/java/com/android/server/connectivity/NetworkMonitor.java#1071
 */
class HttpsTest(private val setStatus: (CharSequence?) -> Unit, private val errorCallback: (CharSequence) -> Unit) {
    private var testCount = 0

    fun testConnection() {
        ++testCount
        setStatus(app.getText(R.string.connection_test_testing))
        val id = testCount  // it would change by other code
        thread("ConnectionTest") {
            val url = URL("https", when (Core.currentProfile!!.route) {
                Acl.CHINALIST -> "www.qualcomm.cn"
                else -> "www.google.com"
            }, "/generate_204")
            val conn = (if (BaseService.usingVpnMode) url.openConnection() else
                url.openConnection(Proxy(Proxy.Type.SOCKS, InetSocketAddress("127.0.0.1", DataStore.portProxy))))
                    as HttpURLConnection
            conn.setRequestProperty("Connection", "close")
            conn.instanceFollowRedirects = false
            conn.useCaches = false
            val (success, result) = try {
                val start = SystemClock.elapsedRealtime()
                val code = conn.responseCode
                val elapsed = SystemClock.elapsedRealtime() - start
                if (code == 204 || code == 200 && conn.responseLength == 0L)
                    Pair(true, app.getString(R.string.connection_test_available, elapsed))
                else throw IOException(app.getString(R.string.connection_test_error_status_code, code))
            } catch (e: IOException) {
                Pair(false, app.getString(R.string.connection_test_error, e.message))
            } finally {
                conn.disconnect()
            }
            if (testCount == id) Core.handler.post {
                if (success) setStatus(result) else {
                    setStatus(app.getText(R.string.connection_test_fail))
                    errorCallback(result)
                }
            }
        }
    }

    fun invalidate() {
        testCount += 1
    }
}
