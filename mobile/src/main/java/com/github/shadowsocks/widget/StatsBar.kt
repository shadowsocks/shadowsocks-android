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
import androidx.activity.viewModels
import androidx.appcompat.widget.TooltipCompat
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.observe
import androidx.lifecycle.whenStarted
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.net.HttpsTest
import com.google.android.material.bottomappbar.BottomAppBar
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class StatsBar @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null,
                                         defStyleAttr: Int = R.attr.bottomAppBarStyle) :
        BottomAppBar(context, attrs, defStyleAttr) {
    private lateinit var statusText: TextView
    private lateinit var txText: TextView
    private lateinit var rxText: TextView
    private lateinit var txRateText: TextView
    private lateinit var rxRateText: TextView
    private val tester by (context as MainActivity).viewModels<HttpsTest>()
    private lateinit var behavior: Behavior
    override fun getBehavior(): Behavior {
        if (!this::behavior.isInitialized) behavior = object : Behavior() {
            override fun onNestedScroll(coordinatorLayout: CoordinatorLayout, child: BottomAppBar, target: View,
                                        dxConsumed: Int, dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int,
                                        type: Int, consumed: IntArray) {
                super.onNestedScroll(coordinatorLayout, child, target, dxConsumed, dyConsumed + dyUnconsumed,
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

    private fun setStatus(text: CharSequence) {
        statusText.text = text
        TooltipCompat.setTooltipText(this, text)
    }

    fun changeState(state: BaseService.State) {
        val activity = context as MainActivity
        fun postWhenStarted(what: () -> Unit) = activity.lifecycleScope.launch(Dispatchers.Main) {
            activity.whenStarted { what() }
        }
        if ((state == BaseService.State.Connected).also { hideOnScroll = it }) {
            postWhenStarted { performShow() }
            tester.status.observe(activity) { it.retrieve(this::setStatus) { msg -> activity.snackbar(msg).show() } }
        } else {
            postWhenStarted { performHide() }
            updateTraffic(0, 0, 0, 0)
            tester.status.removeObservers(activity)
            if (state != BaseService.State.Idle) tester.invalidate()
            setStatus(context.getText(when (state) {
                BaseService.State.Connecting -> R.string.connecting
                BaseService.State.Stopping -> R.string.stopping
                else -> R.string.not_connected
            }))
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
