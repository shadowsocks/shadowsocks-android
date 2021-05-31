/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2021 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2021 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.google.android.material.progressindicator.CircularProgressIndicator

class FabProgressBehavior(context: Context, attrs: AttributeSet?) :
    CoordinatorLayout.Behavior<CircularProgressIndicator>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: CircularProgressIndicator, dependency: View) =
        dependency.id == (child.layoutParams as CoordinatorLayout.LayoutParams).anchorId

    override fun onLayoutChild(parent: CoordinatorLayout, child: CircularProgressIndicator,
                               layoutDirection: Int): Boolean {
        val size = parent.getDependencies(child).single().measuredHeight + child.trackThickness
        return if (child.indicatorSize != size) {
            child.indicatorSize = size
            true
        } else false
    }
}
