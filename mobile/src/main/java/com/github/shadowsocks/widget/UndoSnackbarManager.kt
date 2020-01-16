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

import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.google.android.material.snackbar.Snackbar

/**
 * @param activity MainActivity.
 * //@param view The view to find a parent from.
 * @param undo Callback for undoing removals.
 * @param commit Callback for committing removals.
 * @tparam T Item type.
 */
class UndoSnackbarManager<in T>(private val activity: MainActivity, private val undo: (List<Pair<Int, T>>) -> Unit,
                                commit: ((List<Pair<Int, T>>) -> Unit)? = null) {
    private val recycleBin = ArrayList<Pair<Int, T>>()
    private val removedCallback = object : Snackbar.Callback() {
        override fun onDismissed(transientBottomBar: Snackbar?, event: Int) {
            if (last === transientBottomBar && event != DISMISS_EVENT_ACTION) {
                commit?.invoke(recycleBin)
                recycleBin.clear()
                last = null
            }
        }
    }

    private var last: Snackbar? = null

    fun remove(items: Collection<Pair<Int, T>>) {
        recycleBin.addAll(items)
        val count = recycleBin.size
        activity.snackbar(activity.resources.getQuantityString(R.plurals.removed, count, count)).apply {
            addCallback(removedCallback)
            setAction(R.string.undo) {
                undo(recycleBin.reversed())
                recycleBin.clear()
            }
            last = this
            show()
        }
    }

    fun remove(vararg items: Pair<Int, T>) = remove(items.toList())

    fun flush() = last?.dismiss()
}
