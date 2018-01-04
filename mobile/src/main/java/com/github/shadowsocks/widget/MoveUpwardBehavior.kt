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

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.content.Context
import android.os.Build
import android.support.design.widget.CoordinatorLayout
import android.support.design.widget.SnackbarAnimation
import android.support.design.widget.Snackbar
import android.support.v4.view.ViewCompat
import android.util.AttributeSet
import android.view.View

import com.github.shadowsocks.utils.isAccessibilityEnabled

/**
 * Full credits go to: https://stackoverflow.com/a/35904421/2245107
 */
class MoveUpwardBehavior : CoordinatorLayout.Behavior<View> {
    constructor() : super()
    constructor(context: Context, attrs: AttributeSet) : super(context, attrs)

    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean =
        dependency is Snackbar.SnackbarLayout

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        val params = child.layoutParams
        params.height = parent.height - dependency.height
        child.layoutParams = params
        return true
    }

    /**
     * Based on BaseTransientBottomBar.animateViewOut (support lib 27.0.2).
     */
    override fun onDependentViewRemoved(parent: CoordinatorLayout, child: View, dependency: View) {
        if (!isAccessibilityEnabled(parent.getContext())) {
            val animator = ValueAnimator()
            val start = child.height
            animator.setIntValues(start, parent.height)
            animator.interpolator = SnackbarAnimation.FAST_OUT_SLOW_IN_INTERPOLATOR
            animator.duration = SnackbarAnimation.ANIMATION_DURATION
            animator.addUpdateListener(object : ValueAnimator.AnimatorUpdateListener {
                override fun onAnimationUpdate(animator: ValueAnimator) {
                    val currentValue = animator.animatedValue as Int
                    val params = child.layoutParams
                    params.height = currentValue
                    child.layoutParams = params
                }
            })
            animator.start()
        } else {
            val params = child.layoutParams
            params.height = parent.height
            child.layoutParams = params
        }
    }
}
