/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2016 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2016 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
/*                                                                             */
/*  This program is free software: you can redistribute it and/or modify       */
/*  it under the terms of the GNU General Public License as published by       */
/*  the Free Software Foundation, either version 3 of the License, or          */
/*  (at your option) any later version.                                        */
/*                                                                             */
/*  This program is distributed in the hope that it will be useful,            */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*  GNU General Public License for more details.                               */
/*                                                                             */
/*  You should have received a copy of the GNU General Public License          */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.       */
/*                                                                             */
/*******************************************************************************/

package com.github.shadowsocks.widget

import android.animation.ValueAnimator
import android.content.Context
import android.support.design.widget.CoordinatorLayout
import android.support.design.widget.Snackbar.SnackbarLayout
import android.support.v4.view.animation.FastOutSlowInInterpolator
import android.util.AttributeSet
import android.view.View

import scala.collection.JavaConverters._

/**
  * A SnackbarLayout-aware behavior.
  *
  * TODO: What happened to the animations?
  *
  * @author Mygod
  */
class BottomBehavior(context: Context, attrs: AttributeSet) extends CoordinatorLayout.Behavior[View](context, attrs) {
  private var translationYAnimator: ValueAnimator = _
  private var translationY: Float = _

  override def layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean = dependency match {
    case _: SnackbarLayout => true
    case _ => super.layoutDependsOn(parent, child, dependency)
  }

  override def onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean = {
    dependency match {
      case _: SnackbarLayout =>
        var targetTransY = parent.getDependencies(child).asScala
          .filter(view => view.isInstanceOf[SnackbarLayout] && parent.doViewsOverlap(child, view))
          .map(view => view.getTranslationY - view.getHeight).reduceOption(_ min _).getOrElse(0F)
        if (targetTransY > 0) targetTransY = 0
        if (translationY != targetTransY) {
          val currentTransY = child.getTranslationY
          if (translationYAnimator != null && translationYAnimator.isRunning) translationYAnimator.cancel()
          if (child.isShown && Math.abs(currentTransY - targetTransY) > child.getHeight * 0.667F) {
            if (translationYAnimator == null) {
              translationYAnimator = new ValueAnimator
              translationYAnimator.setInterpolator(new FastOutSlowInInterpolator())
              translationYAnimator.addUpdateListener(animation =>
                child.setTranslationY(animation.getAnimatedValue.asInstanceOf[Float]))
            }
            translationYAnimator.setFloatValues(currentTransY, targetTransY)
            translationYAnimator.start()
          } else child.setTranslationY(targetTransY)
          translationY = targetTransY
        }
        true
      case _ => super.onDependentViewChanged(parent, child, dependency)
    }
  }

  override def onDependentViewRemoved(parent: CoordinatorLayout, child: View, dependency: View): Unit = {
    dependency match {
      case _: SnackbarLayout =>
        if (translationY != 0) {
          val currentTransY = child.getTranslationY
          if (translationYAnimator != null && translationYAnimator.isRunning) translationYAnimator.cancel()
          if (child.isShown && Math.abs(currentTransY) > child.getHeight * 0.667F) {
            if (translationYAnimator == null) {
              translationYAnimator = new ValueAnimator
              translationYAnimator.setInterpolator(new FastOutSlowInInterpolator())
              translationYAnimator.addUpdateListener(animation =>
                child.setTranslationY(animation.getAnimatedValue.asInstanceOf[Float]))
            }
            translationYAnimator.setFloatValues(currentTransY, 0)
            translationYAnimator.start()
          } else child.setTranslationY(0)
          translationY = 0
        }
        parent.dispatchDependentViewsChanged(child)
    }
    super.onDependentViewRemoved(parent, child, dependency)
  }
}
