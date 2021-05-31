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
import androidx.dynamicanimation.animation.DynamicAnimation
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.vectordrawable.graphics.drawable.Animatable2Compat
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat
import com.github.shadowsocks.R
import com.github.shadowsocks.bg.BaseService
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.google.android.material.progressindicator.BaseProgressIndicator
import com.google.android.material.progressindicator.DeterminateDrawable
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import java.util.*

class ServiceButton @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0) :
        FloatingActionButton(context, attrs, defStyleAttr), DynamicAnimation.OnAnimationEndListener {
    companion object {
        private val springAnimator by lazy {
            DeterminateDrawable::class.java.getDeclaredField("springAnimator").apply { isAccessible = true }
        }
    }

    private val callback = object : Animatable2Compat.AnimationCallback() {
        override fun onAnimationEnd(drawable: Drawable) {
            super.onAnimationEnd(drawable)
            var next = animationQueue.peek() ?: return
            if (next.icon.current == drawable) {
                animationQueue.pop()
                next = animationQueue.peek() ?: return
            }
            next.start()
        }
    }

    private inner class AnimatedState(@DrawableRes resId: Int,
                                      private val onStart: BaseProgressIndicator<*>.() -> Unit = { hideProgress() }) {
        val icon: AnimatedVectorDrawableCompat = AnimatedVectorDrawableCompat.create(context, resId)!!.apply {
            registerAnimationCallback(this@ServiceButton.callback)
        }
        fun start() {
            setImageDrawable(icon)
            icon.start()
            progress.onStart()
        }
        fun stop() = icon.stop()
    }

    private val iconStopped by lazy { AnimatedState(R.drawable.ic_service_stopped) }
    private val iconConnecting by lazy {
        AnimatedState(R.drawable.ic_service_connecting) {
            hideProgress()
            delayedAnimation = (context as LifecycleOwner).lifecycleScope.launchWhenStarted {
                delay(context.resources.getInteger(android.R.integer.config_mediumAnimTime) + 1000L)
                isIndeterminate = true
                show()
            }
        }
    }
    private val iconConnected by lazy {
        AnimatedState(R.drawable.ic_service_connected) {
            delayedAnimation?.cancel()
            setProgressCompat(1, true)
        }
    }
    private val iconStopping by lazy { AnimatedState(R.drawable.ic_service_stopping) }
    private val animationQueue = ArrayDeque<AnimatedState>()

    private var checked = false
    private var delayedAnimation: Job? = null
    private lateinit var progress: BaseProgressIndicator<*>
    fun initProgress(progress: BaseProgressIndicator<*>) {
        this.progress = progress
        (springAnimator.get(progress.progressDrawable) as DynamicAnimation<*>).addEndListener(this)
    }
    override fun onAnimationEnd(animation: DynamicAnimation<out DynamicAnimation<*>>?, canceled: Boolean, value: Float,
                                velocity: Float) {
        if (!canceled) progress.hide()
    }

    private fun hideProgress() {
        delayedAnimation?.cancel()
        progress.hide()
    }

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

    private fun changeState(icon: AnimatedState, animate: Boolean) {
        fun counters(a: AnimatedState, b: AnimatedState): Boolean =
                a == iconStopped && b == iconConnecting ||
                a == iconConnecting && b == iconStopped ||
                a == iconConnected && b == iconStopping ||
                a == iconStopping && b == iconConnected
        if (animate) {
            if (animationQueue.size < 2 || !counters(animationQueue.last, icon)) {
                animationQueue.add(icon)
                if (animationQueue.size == 1) icon.start()
            } else animationQueue.removeLast()
        } else {
            animationQueue.peekFirst()?.stop()
            animationQueue.clear()
            icon.start()    // force ensureAnimatorSet to be called so that stop() will work
            icon.stop()
        }
    }
}
