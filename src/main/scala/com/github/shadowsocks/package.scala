package com.github

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.util.{Failure, Try}

/**
  * @author Mygod
  */
package object shadowsocks {
  private val handleFailure: Try[_] => Unit = {
    case Failure(e) => e.printStackTrace()
    case _ =>
  }

  def ThrowableFuture[T](f: => T) = Future(f) onComplete handleFailure
}
