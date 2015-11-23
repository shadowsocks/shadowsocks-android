package com.github.shadowsocks.widget

import android.content.Context
import android.support.design.widget.CoordinatorLayout
import android.support.design.widget.CoordinatorLayout.Behavior
import android.support.design.widget.Snackbar.SnackbarLayout
import android.util.AttributeSet
import android.view.View

/**
  * @author Mygod
  */
class AutoPaddingBehavior(context: Context, attrs: AttributeSet) extends Behavior[View] {
  override def layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View) =
    dependency.isInstanceOf[SnackbarLayout]

  override def onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View) =
    dependency match {
      case sl: SnackbarLayout =>
        child.setPadding(0, 0, 0, dependency.getHeight)
        true
      case _ => super.onDependentViewChanged(parent, child, dependency)
    }

  override def onDependentViewRemoved(parent: CoordinatorLayout, child: View, dependency: View) =
    dependency match {
      case sl: SnackbarLayout => child.setPadding(0, 0, 0, 0)
      case _ => super.onDependentViewRemoved(parent, child, dependency)
    }
}
