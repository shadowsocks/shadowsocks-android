package com.github.shadowsocks.widget

import android.content.Context
import android.support.v7.widget.GridLayout
import android.util.AttributeSet

/**
  * @author Mygod
  */
class BoundedGridLayout(context: Context, attrs: AttributeSet) extends GridLayout(context, attrs) with BoundedView {
  initAttrs(context, attrs)
}
