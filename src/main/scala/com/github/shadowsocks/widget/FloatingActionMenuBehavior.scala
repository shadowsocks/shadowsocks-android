package com.github.shadowsocks.widget

import android.animation.ValueAnimator
import android.content.Context
import android.support.design.widget.CoordinatorLayout
import android.support.design.widget.CoordinatorLayout.Behavior
import android.support.design.widget.Snackbar.SnackbarLayout
import android.support.v4.view.animation.FastOutSlowInInterpolator
import android.util.AttributeSet
import android.view.View
import com.github.clans.fab.FloatingActionMenu

import scala.collection.JavaConverters._

/**
  * Behavior for com.github.clans.fab.FloatingActionMenu that is aware of Snackbars and scrolling.
  *
  * @author Mygod
  */
class FloatingActionMenuBehavior(context: Context, attrs: AttributeSet)
  extends Behavior[FloatingActionMenu](context, attrs) {
  private var fabTranslationYAnimator: ValueAnimator = _
  private var fabTranslationY: Float = _

  override def layoutDependsOn(parent: CoordinatorLayout, child: FloatingActionMenu, dependency: View) =
    dependency.isInstanceOf[SnackbarLayout]

  override def onDependentViewChanged(parent: CoordinatorLayout, child: FloatingActionMenu, dependency: View) = {
    dependency match {
      case _: SnackbarLayout =>
        var targetTransY = parent.getDependencies(child).asScala
          .filter(view => view.isInstanceOf[SnackbarLayout] && parent.doViewsOverlap(child, view))
          .map(view => view.getTranslationY - view.getHeight).reduceOption(_ min _).getOrElse(0F)
        if (targetTransY > 0) targetTransY = 0
        if (fabTranslationY != targetTransY) {
          val currentTransY = child.getTranslationY
          if (fabTranslationYAnimator != null && fabTranslationYAnimator.isRunning) fabTranslationYAnimator.cancel
          if (child.isShown && Math.abs(currentTransY - targetTransY) > child.getHeight * 0.667F) {
            if (fabTranslationYAnimator == null) {
              fabTranslationYAnimator = new ValueAnimator
              fabTranslationYAnimator.setInterpolator(new FastOutSlowInInterpolator)
              fabTranslationYAnimator.addUpdateListener(animation =>
                child.setTranslationY(animation.getAnimatedValue.asInstanceOf[Float]))
            }
            fabTranslationYAnimator.setFloatValues(currentTransY, targetTransY)
            fabTranslationYAnimator.start
          } else child.setTranslationY(targetTransY)
          fabTranslationY = targetTransY
        }
    }
    false
  }

  override def onStartNestedScroll(parent: CoordinatorLayout, child: FloatingActionMenu, directTargetChild: View,
                                   target: View, nestedScrollAxes: Int) = true
  override def onNestedScroll(parent: CoordinatorLayout, child: FloatingActionMenu, target: View, dxConsumed: Int,
                              dyConsumed: Int, dxUnconsumed: Int, dyUnconsumed: Int) = {
    super.onNestedScroll(parent, child, target, dxConsumed, dyConsumed, dxUnconsumed, dyUnconsumed)
    val dy = dyConsumed + dyUnconsumed
    if (child.isMenuButtonHidden) {
      if (dy < 0) child.showMenuButton(true)
    } else if (dy > 0) child.hideMenuButton(true)
  }
}
