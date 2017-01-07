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

package com.github.shadowsocks.utils

/**
  * @author Mygod
  */
object CloseUtils {
  type Disconnectable = {
    def disconnect()
  }

  def autoClose[A <: AutoCloseable, B](x: => A)(block: A => B): B = {
    var a: Option[A] = None
    try {
      a = Some(x)
      block(a.get)
    } finally if (a.nonEmpty) try {
      val v = a.get
      if (v ne null) v.close()
    } catch {
      case _: Exception =>
    }
  }
  def autoDisconnect[A <: Disconnectable, B](x: => A)(block: A => B): B = {
    var a: Option[A] = None
    try {
      a = Some(x)
      block(a.get)
    } finally if (a.nonEmpty) try {
      val v = a.get
      if (v ne null) v.disconnect()
    } catch {
      case _: Exception =>
    }
  }
}
