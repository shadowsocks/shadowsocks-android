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
    override def onDismissed(snackbar: Snackbar, event: Int) = {
      event match {
        case Snackbar.Callback.DISMISS_EVENT_SWIPE | Snackbar.Callback.DISMISS_EVENT_MANUAL |
             Snackbar.Callback.DISMISS_EVENT_TIMEOUT =>
          if (commit != null) commit(recycleBin.iterator)
          recycleBin.clear
        case _ =>
      }
      last = null
    }
  }
  private var last: Snackbar = _

  def remove(index: Int, item: T) = {
    recycleBin.append((index, item))
    val count = recycleBin.length
    last = Snackbar
      .make(view, view.getResources.getQuantityString(R.plurals.removed, count, count: Integer), Snackbar.LENGTH_LONG)
      .setCallback(removedCallback).setAction(R.string.undo, (_ => {
      undo(recycleBin.reverseIterator)
      recycleBin.clear
    }): View.OnClickListener)
    last.show
  }

  def flush = if (last != null) last.dismiss
}
