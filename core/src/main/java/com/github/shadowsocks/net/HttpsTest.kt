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

package com.github.shadowsocks.net

import android.os.Build
import android.os.SystemClock
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.github.shadowsocks.Core.app
import com.github.shadowsocks.core.R
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.Key
import com.github.shadowsocks.utils.useCancellable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.io.IOException
import java.net.HttpURLConnection
import java.net.Proxy
import java.net.URL
import java.net.URLConnection

/**
 * Based on: https://android.googlesource.com/platform/frameworks/base/+/b19a838/services/core/java/com/android/server/connectivity/NetworkMonitor.java#1071
 */
class HttpsTest : ViewModel() {
    sealed class Status {
        protected abstract val status: CharSequence
        open fun retrieve(setStatus: (CharSequence) -> Unit, errorCallback: (String) -> Unit) = setStatus(status)

        object Idle : Status() {
            override val status get() = app.getText(R.string.vpn_connected)
        }
        object Testing : Status() {
            override val status get() = app.getText(R.string.connection_test_testing)
        }
        class Success(private val elapsed: Long) : Status() {
            override val status get() = app.getString(R.string.connection_test_available, elapsed)
        }
        sealed class Error : Status() {
            override val status get() = app.getText(R.string.connection_test_fail)
            protected abstract val error: String
            private var shown = false
            override fun retrieve(setStatus: (CharSequence) -> Unit, errorCallback: (String) -> Unit) {
                super.retrieve(setStatus, errorCallback)
                if (shown) return
                shown = true
                errorCallback(error)
            }

            class UnexpectedResponseCode(private val code: Int) : Error() {
                override val error get() = app.getString(R.string.connection_test_error_status_code, code)
            }
            class IOFailure(private val e: IOException) : Error() {
                override val error get() = app.getString(R.string.connection_test_error, e.message)
            }
        }
    }

    private var running: Job? = null
    val status = MutableLiveData<Status>(Status.Idle)

    fun testConnection() {
        cancelTest()
        status.value = Status.Testing
        val url = URL("https://detectportal.firefox.com/success.txt")
        val conn = (if (DataStore.serviceMode != Key.modeVpn) {
            url.openConnection(Proxy(Proxy.Type.SOCKS, DataStore.proxyAddress))
        } else url.openConnection()) as HttpURLConnection
        conn.setRequestProperty("Connection", "close")
        conn.instanceFollowRedirects = false
        conn.useCaches = false
        running = GlobalScope.launch(Dispatchers.Main.immediate) {
            status.value = conn.useCancellable {
                try {
                    val start = SystemClock.elapsedRealtime()
                    val code = responseCode
                    val elapsed = SystemClock.elapsedRealtime() - start
                    if (code == 204 || code == 200 && responseLength == 0L) Status.Success(elapsed)
                    else Status.Error.UnexpectedResponseCode(code)
                } catch (e: IOException) {
                    Status.Error.IOFailure(e)
                } finally {
                    disconnect()
                }
            }
        }
    }

    private fun cancelTest() {
        running?.cancel()
        running = null
    }

    fun invalidate() {
        cancelTest()
        status.value = Status.Idle
    }

    private val URLConnection.responseLength: Long
        get() = if (Build.VERSION.SDK_INT >= 24) contentLengthLong else contentLength.toLong()
}
