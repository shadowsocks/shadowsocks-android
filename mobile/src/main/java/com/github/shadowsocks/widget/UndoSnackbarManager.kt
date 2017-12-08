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

import android.support.design.widget.Snackbar
import android.view.View
import com.github.shadowsocks.R

/**
 * @param view The view to find a parent from.
 * @param undo Callback for undoing removals.
 * @param commit Callback for committing removals.
 * @tparam T Item type.
 */
class UndoSnackbarManager<in T>(private val view: View, private val undo: (List<Pair<Int, T>>) -> Unit,
                                commit: ((List<Pair<Int, T>>) -> Unit)? = null) {
    private val recycleBin = ArrayList<Pair<Int, T>>()
    private val removedCallback = object : Snackbar.Callback() {
        override fun onDismissed(transientBottomBar: Snackbar?, event: Int) {
            when (event) {
                DISMISS_EVENT_SWIPE, DISMISS_EVENT_MANUAL, DISMISS_EVENT_TIMEOUT -> {
                    commit?.invoke(recycleBin)
                    recycleBin.clear()
                }
            }
            last = null
        }
    }
    private var last: Snackbar? = null

    fun remove(items: Collection<Pair<Int, T>>) {
        recycleBin.addAll(items)
        val count = recycleBin.size
        val snackbar = Snackbar
                .make(view, view.resources.getQuantityString(R.plurals.removed, count, count), Snackbar.LENGTH_LONG)
                .addCallback(removedCallback)
                .setAction(R.string.undo, {
                    undo(recycleBin.reversed())
                    recycleBin.clear()
                })
        snackbar.show()
        last = snackbar
    }
    fun remove(vararg items: Pair<Int, T>) = remove(items.toList())

    fun flush() {
        last?.dismiss()
        last = null
    }
}
