package com.github

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.util.{Success, Failure, Try}

/**
  * @author Mygod
  */
package object shadowsocks {

  val handleFailure: PartialFunction[Try[_], Unit] = {
    case Success(_) =>
    case Failure(e) => e.printStackTrace()
  }

  def ThrowableFuture[T](f: => T) = Future(f) onComplete handleFailure
}