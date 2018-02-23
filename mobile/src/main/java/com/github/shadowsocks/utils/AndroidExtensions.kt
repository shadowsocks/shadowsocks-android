package com.github.shadowsocks.utils

import android.support.annotation.StringRes
import android.support.design.widget.Snackbar
import android.view.View

fun View.snack(message: Int, length: Int = Snackbar.LENGTH_LONG) {
    val snack = Snackbar.make(this, context.getString(message), length)
    snack.show()
}

fun View.snack(message: CharSequence, length: Int = Snackbar.LENGTH_LONG) {
    val snack = Snackbar.make(this, message, length)
    snack.show()
}
