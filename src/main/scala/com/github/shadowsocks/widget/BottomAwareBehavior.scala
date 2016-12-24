package com.github.shadowsocks.widget

import android.content.Context
import android.support.design.widget.CoordinatorLayout
import android.util.AttributeSet
import android.view.View

/**
  * Behavior for content area that's aware of bottom bars.
  *
  * @author Mygod
  */
class BottomAwareBehavior(context: Context, attrs: AttributeSet)
  extends CoordinatorLayout.Behavior[View](context, attrs) {
  override def onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean = {
    val params = child.getLayoutParams.asInstanceOf[CoordinatorLayout.LayoutParams]
    val margin = parent.getHeight - dependency.getTop - dependency.getTranslationY.toInt
    if (params.bottomMargin != margin) {
      params.bottomMargin = margin
      true
    } else false
  }
}
