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
