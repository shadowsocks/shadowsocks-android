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
