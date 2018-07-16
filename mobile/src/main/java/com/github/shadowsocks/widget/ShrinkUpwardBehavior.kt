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

import android.animation.ValueAnimator
import android.content.Context
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.google.android.material.snackbar.Snackbar
import com.google.android.material.snackbar.SnackbarConsts
import android.util.AttributeSet
import android.view.View
import android.view.accessibility.AccessibilityManager
import androidx.core.content.getSystemService
import com.google.android.material.animation.AnimationUtils

/**
 * Full credits go to: https://stackoverflow.com/a/35904421/2245107
 */
class ShrinkUpwardBehavior(context: Context, attrs: AttributeSet?) : CoordinatorLayout.Behavior<View>(context, attrs) {
    private val accessibility = context.getSystemService<AccessibilityManager>()!!

    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean =
            dependency is Snackbar.SnackbarLayout

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        child.layoutParams.height = dependency.y.toInt()
        child.requestLayout()
        return true
    }

    /**
     * Based on: https://android.googlesource.com/platform/frameworks/support.git/+/fa0f82f/design/src/android/support/design/widget/BaseTransientBottomBar.java#558
     */
    override fun onDependentViewRemoved(parent: CoordinatorLayout, child: View, dependency: View) {
        if (accessibility.isEnabled) child.layoutParams.height = parent.height else {
            val animator = ValueAnimator()
            val start = child.height
            animator.setIntValues(start, parent.height)
            animator.interpolator = AnimationUtils.FAST_OUT_SLOW_IN_INTERPOLATOR
            animator.duration = SnackbarConsts.ANIMATION_DURATION
            animator.addUpdateListener {
                child.layoutParams.height = it.animatedValue as Int
                child.requestLayout()
            }
            animator.start()
        }
    }
}
