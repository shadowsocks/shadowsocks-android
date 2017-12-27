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

package com.github.shadowsocks.widget

import android.content.Context
import android.graphics.drawable.Drawable
import android.support.annotation.DrawableRes
import android.support.design.widget.FloatingActionButton
import android.support.graphics.drawable.Animatable2Compat
import android.support.graphics.drawable.AnimatedVectorDrawableCompat
import android.support.v7.widget.TooltipCompat
import android.util.AttributeSet
import android.view.View
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import java.util.*

class ServiceButton @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0) :
        FloatingActionButton(context, attrs, defStyleAttr) {
    private val callback = object : Animatable2Compat.AnimationCallback() {
        override fun onAnimationEnd(drawable: Drawable) {
            super.onAnimationEnd(drawable)
            var next = animationQueue.peek() ?: return
            if (next.current == drawable) {
                animationQueue.pop()
                next = animationQueue.peek() ?: return
            }
            setImageDrawable(next)
            next.start()
        }
    }

    private fun createIcon(@DrawableRes resId: Int): AnimatedVectorDrawableCompat {
        val result = AnimatedVectorDrawableCompat.create(context, resId)!!
        result.registerAnimationCallback(callback)
        return result
    }

    private val iconStopped by lazy { createIcon(R.drawable.ic_service_stopped) }
    private val iconConnecting by lazy { createIcon(R.drawable.ic_service_connecting) }
    private val iconConnected by lazy { createIcon(R.drawable.ic_service_connected) }
    private val iconStopping by lazy { createIcon(R.drawable.ic_service_stopping) }
    private val animationQueue = ArrayDeque<AnimatedVectorDrawableCompat>()

    private var checked = false

    override fun onCreateDrawableState(extraSpace: Int): IntArray {
        val drawableState = super.onCreateDrawableState(extraSpace + 1)
        if (checked) View.mergeDrawableStates(drawableState, intArrayOf(android.R.attr.state_checked))
        return drawableState
    }

    fun changeState(state: Int, animate: Boolean) {
        when (state) {
            BaseService.CONNECTING -> changeState(iconConnecting, animate)
            BaseService.CONNECTED -> changeState(iconConnected, animate)
            BaseService.STOPPING -> changeState(iconStopping, animate)
            else -> changeState(iconStopped, animate)
        }
        if (state == BaseService.CONNECTED) {
            checked = true
            TooltipCompat.setTooltipText(this, context.getString(R.string.stop))
        } else {
            checked = false
            TooltipCompat.setTooltipText(this, context.getString(R.string.connect))
        }
        refreshDrawableState()
        isEnabled = false
        if (state == BaseService.CONNECTED || state == BaseService.STOPPED) app.handler.postDelayed(
                { isEnabled = state == BaseService.CONNECTED || state == BaseService.STOPPED }, 1000)
    }

    private fun counters(a: AnimatedVectorDrawableCompat, b: AnimatedVectorDrawableCompat): Boolean =
            a == iconStopped && b == iconConnecting ||
            a == iconConnecting && b == iconStopped ||
            a == iconConnected && b == iconStopping ||
            a == iconStopping && b == iconConnected

    private fun changeState(icon: AnimatedVectorDrawableCompat, animate: Boolean) {
        if (animate) {
            if (animationQueue.size < 2 || !counters(animationQueue.last, icon)) {
                animationQueue.add(icon)
                if (animationQueue.size == 1) {
                    setImageDrawable(icon)
                    icon.start()
                }
            } else animationQueue.removeLast()
        } else {
            animationQueue.peekFirst()?.stop()
            animationQueue.clear()
            setImageDrawable(icon)
            icon.start()    // force ensureAnimatorSet to be called so that stop() will work
            icon.stop()
        }
    }
}
