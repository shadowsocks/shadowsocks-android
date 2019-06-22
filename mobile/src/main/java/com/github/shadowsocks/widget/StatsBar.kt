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
import android.text.format.Formatter
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.get
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.net.HttpsTest
import com.google.android.material.bottomappbar.BottomAppBar
import kotlin.math.abs

class StatsBar @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null,
                                         defStyleAttr: Int = R.attr.bottomAppBarStyle) :
        BottomAppBar(context, attrs, defStyleAttr) {
    private lateinit var statusText: TextView
    private lateinit var txText: TextView
    private lateinit var rxText: TextView
    private lateinit var txRateText: TextView
    private lateinit var rxRateText: TextView
    private val tester = ViewModelProviders.of(context as FragmentActivity).get<HttpsTest>()
    private lateinit var behavior: Behavior
    override fun getBehavior(): Behavior {
        if (!this::behavior.isInitialized) behavior = object : Behavior() {
            val threshold = context.resources.getDimensionPixelSize(R.dimen.stats_bar_scroll_threshold)
            override fun onNestedScroll(coordinatorLayout: CoordinatorLayout, child: BottomAppBar, target: View,
                                        dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int,
                                        type: Int, consumed: IntArray) {
                val dy = dyConsumed + dyUnconsumed
                super.onNestedScroll(coordinatorLayout, child, target, dxConsumed, if (abs(dy) >= threshold) dy else 0,
                        dxUnconsumed, 0, type, consumed)
            }
        }
        return behavior
    }

    override fun setOnClickListener(l: OnClickListener?) {
        statusText = findViewById(R.id.status)
        txText = findViewById(R.id.tx)
        txRateText = findViewById(R.id.txRate)
        rxText = findViewById(R.id.rx)
        rxRateText = findViewById(R.id.rxRate)
        super.setOnClickListener(l)
    }

    fun changeState(state: BaseService.State) {
        val activity = context as MainActivity
        if (state != BaseService.State.Connected) {
            updateTraffic(0, 0, 0, 0)
            tester.status.removeObservers(activity)
            if (state != BaseService.State.Idle) tester.invalidate()
            statusText.setText(when (state) {
                BaseService.State.Connecting -> R.string.connecting
                BaseService.State.Stopping -> R.string.stopping
                else -> R.string.not_connected
            })
        } else {
            behavior.slideUp(this)
            tester.status.observe(activity, Observer {
                it.retrieve(statusText::setText) { activity.snackbar(it).show() }
            })
        }
    }

    fun updateTraffic(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) {
        txText.text = "▲ ${Formatter.formatFileSize(context, txTotal)}"
        rxText.text = "▼ ${Formatter.formatFileSize(context, rxTotal)}"
        txRateText.text = context.getString(R.string.speed, Formatter.formatFileSize(context, txRate))
        rxRateText.text = context.getString(R.string.speed, Formatter.formatFileSize(context, rxRate))
    }

    fun testConnection() = tester.testConnection()
}
