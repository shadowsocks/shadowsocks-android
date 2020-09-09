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
import android.os.Build
import android.util.AttributeSet
import android.view.PointerIcon
import android.view.View
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.TooltipCompat
import androidx.vectordrawable.graphics.drawable.Animatable2Compat
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.google.android.material.floatingactionbutton.FloatingActionButton
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

    fun changeState(state: BaseService.State, previousState: BaseService.State, animate: Boolean) {
        when (state) {
            BaseService.State.Connecting -> changeState(iconConnecting, animate)
            BaseService.State.Connected -> changeState(iconConnected, animate)
            BaseService.State.Stopping -> {
                changeState(iconStopping, animate && previousState == BaseService.State.Connected)
            }
            else -> changeState(iconStopped, animate)
        }
        checked = state == BaseService.State.Connected
        refreshDrawableState()
        val description = context.getText(if (state.canStop) R.string.stop else R.string.connect)
        contentDescription = description
        TooltipCompat.setTooltipText(this, description)
        val enabled = state.canStop || state == BaseService.State.Stopped
        isEnabled = enabled
        if (Build.VERSION.SDK_INT >= 24) pointerIcon = PointerIcon.getSystemIcon(context,
                if (enabled) PointerIcon.TYPE_HAND else PointerIcon.TYPE_WAIT)
    }

    private fun changeState(icon: AnimatedVectorDrawableCompat, animate: Boolean) {
        fun counters(a: AnimatedVectorDrawableCompat, b: AnimatedVectorDrawableCompat): Boolean =
                a == iconStopped && b == iconConnecting ||
                a == iconConnecting && b == iconStopped ||
                a == iconConnected && b == iconStopping ||
                a == iconStopping && b == iconConnected
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
