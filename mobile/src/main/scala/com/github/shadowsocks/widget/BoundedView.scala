/*******************************************************************************/
/*                                                                             */
/*  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          */
/*  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  */
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
import android.util.AttributeSet
import android.view.View
import android.view.View.MeasureSpec
import com.github.shadowsocks.R

/**
  * Based on: http://stackoverflow.com/a/6212120/2245107
  *
  * @author Mygod
  */
trait BoundedView extends View {
  private var boundedWidth: Int = _
  private var boundedHeight: Int = _

  protected def initAttrs(context: Context, attrs: AttributeSet) {
    val arr = context.obtainStyledAttributes(attrs, R.styleable.BoundedView)
    boundedWidth = arr.getDimensionPixelSize(R.styleable.BoundedView_bounded_width, 0)
    boundedHeight = arr.getDimensionPixelSize(R.styleable.BoundedView_bounded_height, 0)
    arr.recycle()
  }

  override def onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int): Unit = super.onMeasure(
    if (boundedWidth > 0 && boundedWidth < MeasureSpec.getSize(widthMeasureSpec))
      MeasureSpec.makeMeasureSpec(boundedWidth, MeasureSpec.getMode(widthMeasureSpec))
    else widthMeasureSpec,
    if (boundedHeight > 0 && boundedHeight < MeasureSpec.getSize(heightMeasureSpec))
      MeasureSpec.makeMeasureSpec(boundedHeight, MeasureSpec.getMode(heightMeasureSpec))
    else heightMeasureSpec)
}
