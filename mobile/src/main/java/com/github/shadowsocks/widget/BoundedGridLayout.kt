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
import android.support.v7.widget.GridLayout
import android.util.AttributeSet
import com.github.shadowsocks.R

/**
 * Based on: http://stackoverflow.com/a/6212120/2245107
 */
class BoundedGridLayout @JvmOverloads constructor(context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0,
                                                  defStyleRes: Int = 0) : GridLayout(context, attrs, defStyleAttr) {
    private val boundedWidth: Int
    private val boundedHeight: Int

    init {
        val arr = context.obtainStyledAttributes(attrs, R.styleable.BoundedGridLayout, defStyleAttr, defStyleRes)
        boundedWidth = arr.getDimensionPixelSize(R.styleable.BoundedGridLayout_bounded_width, 0)
        boundedHeight = arr.getDimensionPixelSize(R.styleable.BoundedGridLayout_bounded_height, 0)
        arr.recycle()
    }

    override fun onMeasure(widthSpec: Int, heightSpec: Int) = super.onMeasure(
            if (boundedWidth <= 0 || boundedWidth >= MeasureSpec.getSize(widthSpec)) widthSpec
            else MeasureSpec.makeMeasureSpec(boundedWidth, MeasureSpec.getMode(widthSpec)),
            if (boundedHeight <= 0 || boundedHeight >= MeasureSpec.getSize(heightSpec)) heightSpec
            else MeasureSpec.makeMeasureSpec(boundedHeight, MeasureSpec.getMode(heightSpec)))
}
