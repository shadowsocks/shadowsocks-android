package android.support.design.widget

import android.view.animation.Interpolator

object SnackbarAnimation {
    val FAST_OUT_SLOW_IN_INTERPOLATOR: Interpolator get() = AnimationUtils.FAST_OUT_SLOW_IN_INTERPOLATOR
    const val ANIMATION_DURATION = BaseTransientBottomBar.ANIMATION_DURATION.toLong()
}
