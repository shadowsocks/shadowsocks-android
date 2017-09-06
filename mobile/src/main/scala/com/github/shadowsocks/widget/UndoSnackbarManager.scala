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

import android.support.design.widget.Snackbar
import android.view.View
import com.github.shadowsocks.R

import scala.collection.mutable.ArrayBuffer

/**
  * @author Mygod
  * @param view The view to find a parent from.
  * @param undo Callback for undoing removals.
  * @param commit Callback for committing removals.
  * @tparam T Item type.
  */
class UndoSnackbarManager[T](view: View, undo: Iterator[(Int, T)] => Unit,
                             commit: Iterator[(Int, T)] => Unit = null) {
  private val recycleBin = new ArrayBuffer[(Int, T)]
  private val removedCallback = new Snackbar.Callback {
    override def onDismissed(snackbar: Snackbar, event: Int) {
      event match {
        case Snackbar.Callback.DISMISS_EVENT_SWIPE | Snackbar.Callback.DISMISS_EVENT_MANUAL |
             Snackbar.Callback.DISMISS_EVENT_TIMEOUT =>
          if (commit != null) commit(recycleBin.iterator)
          recycleBin.clear()
        case _ =>
      }
      last = null
    }
  }
  private var last: Snackbar = _

  def remove(items: (Int, T)*) {
    recycleBin.appendAll(items)
    val count = recycleBin.length
    last = Snackbar
      .make(view, view.getResources.getQuantityString(R.plurals.removed, count, count: Integer), Snackbar.LENGTH_LONG)
      .addCallback(removedCallback)
      .setAction(R.string.undo, (_ => {
        undo(recycleBin.reverseIterator)
        recycleBin.clear
      }): View.OnClickListener)
    last.show()
  }

  def flush(): Unit = if (last != null) last.dismiss()
}
