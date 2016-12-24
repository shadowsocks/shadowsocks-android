package com.github.shadowsocks.widget

import android.content.Context
import android.support.v7.widget.CardView
import android.util.AttributeSet

/**
  * @author Mygod
  */
class BoundedCardView(context: Context, attrs: AttributeSet) extends CardView(context, attrs) with BoundedView {
  initAttrs(context, attrs)
}
