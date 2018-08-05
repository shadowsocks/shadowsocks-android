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

package com.github.shadowsocks.widget

import android.content.Context
import android.os.SystemClock
import android.text.format.Formatter
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.responseLength
import com.github.shadowsocks.utils.thread
import com.google.android.material.bottomappbar.BottomAppBar
import java.io.IOException
import java.net.HttpURLConnection
import java.net.InetSocketAddress
import java.net.Proxy
import java.net.URL

class StatsBar @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null,
                                         defStyleAttr: Int = R.attr.bottomAppBarStyle) :
        BottomAppBar(context, attrs, defStyleAttr) {
    private var testCount = 0
    private lateinit var statusText: TextView
    private lateinit var txText: TextView
    private lateinit var rxText: TextView
    private lateinit var txRateText: TextView
    private lateinit var rxRateText: TextView

    override fun getBehavior() = object : Behavior() {
        val threshold = context.resources.getDimensionPixelSize(R.dimen.stats_bar_scroll_threshold)
        override fun onNestedScroll(coordinatorLayout: CoordinatorLayout, child: BottomAppBar, target: View,
                                    dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int, type: Int) {
            val dy = dyConsumed + dyUnconsumed
            super.onNestedScroll(coordinatorLayout, child, target, dxConsumed, if (Math.abs(dy) >= threshold) dy else 0,
                    dxUnconsumed, 0, type)
        }
    }

    override fun setOnClickListener(l: OnClickListener?) {
        statusText = findViewById(R.id.status)
        txText = findViewById(R.id.tx)
        txRateText = findViewById(R.id.txRate)
        rxText = findViewById(R.id.rx)
        rxRateText = findViewById(R.id.rxRate)
        super.setOnClickListener(l)
    }

    fun changeState(state: Int) {
        statusText.setText(when (state) {
            BaseService.CONNECTING -> R.string.connecting
            BaseService.CONNECTED -> R.string.vpn_connected
            BaseService.STOPPING -> R.string.stopping
            else -> R.string.not_connected
        })
        if (state != BaseService.CONNECTED) {
            updateTraffic(0, 0, 0, 0)
            testCount += 1  // suppress previous test messages
        }
    }

    fun updateTraffic(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        txText.text = "▲ ${Formatter.formatFileSize(context, txTotal)}"
        rxText.text = "▼ ${Formatter.formatFileSize(context, rxTotal)}"
        txRateText.text = context.getString(R.string.speed, Formatter.formatFileSize(context, txRate))
        rxRateText.text = context.getString(R.string.speed, Formatter.formatFileSize(context, rxRate))
    }

    /**
     * Based on: https://android.googlesource.com/platform/frameworks/base/+/b19a838/services/core/java/com/android/server/connectivity/NetworkMonitor.java#1071
     */
    fun testConnection() {
        ++testCount
        statusText.setText(R.string.connection_test_testing)
        val id = testCount  // it would change by other code
        thread("ConnectionTest") {
            val url = URL("https", when (app.currentProfile!!.route) {
                Acl.CHINALIST -> "www.qualcomm.cn"
                else -> "www.google.com"
            }, "/generate_204")
            val conn = (if (BaseService.usingVpnMode) url.openConnection() else
                url.openConnection(Proxy(Proxy.Type.SOCKS,
                        InetSocketAddress("127.0.0.1", DataStore.portProxy))))
                    as HttpURLConnection
            conn.setRequestProperty("Connection", "close")
            conn.instanceFollowRedirects = false
            conn.useCaches = false
            val context = context as MainActivity
            val (success, result) = try {
                val start = SystemClock.elapsedRealtime()
                val code = conn.responseCode
                val elapsed = SystemClock.elapsedRealtime() - start
                if (code == 204 || code == 200 && conn.responseLength == 0L)
                    Pair(true, context.getString(R.string.connection_test_available, elapsed))
                else throw IOException(context.getString(R.string.connection_test_error_status_code, code))
            } catch (e: IOException) {
                Pair(false, context.getString(R.string.connection_test_error, e.message))
            } finally {
                conn.disconnect()
            }
            if (testCount == id) app.handler.post {
                if (success) statusText.text = result else {
                    statusText.setText(R.string.connection_test_fail)
                    context.snackbar(result).show()
                }
            }
        }
    }
}
