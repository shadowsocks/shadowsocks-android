package com.github.shadowsocks.utils

import android.util.Log

/**
  * @author chentaov5@gmail.com
  */
object IOUtils {

  val TAG = "IOUtils"

  def autoClose[R <: {def close() : Unit}, T](in: R)(fun: => T): T = {
    try {
      fun
    } finally if (in ne null)
      try {
        in.close()
      } catch {
        case e: Exception => Log.d(TAG, e.getMessage, e)
      }
  }

  def inSafe[T](fun: => T): Option[T] = {
    var res: Option[T] = None
    try {
      res = Some(fun)
    } catch {
      case e: Exception => Log.d(TAG, e.getMessage, e)
    }
    res
  }

}
