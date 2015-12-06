package com.github

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future

/**
  * @author Mygod
  */
package object shadowsocks {
  def ThrowableFuture[T](f: => T) = Future(f) onFailure {
    case e: Throwable => e.printStackTrace()
  }
}
