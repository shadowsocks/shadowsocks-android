package com.github.shadowsocks.utils

/**
  * @author Mygod
  */
object CloseUtils {
  type Closeable = {
    def close()
  }
  type Disconnectable = {
    def disconnect()
  }

  def autoClose[A <: Closeable, B](x: => A)(block: A => B): B = {
    var a: Option[A] = None
    try {
      a = Some(x)
      block(a.get)
    } finally if (a.nonEmpty) try {
      val v = a.get
      if (v ne null) v.close
    } catch {
      case ignore: Exception =>
    }
  }
  def autoDisconnect[A <: Disconnectable, B](x: => A)(block: A => B): B = {
    var a: Option[A] = None
    try {
      a = Some(x)
      block(a.get)
    } finally if (a.nonEmpty) try {
      val v = a.get
      if (v ne null) v.disconnect
    } catch {
      case ignore: Exception =>
    }
  }
}
