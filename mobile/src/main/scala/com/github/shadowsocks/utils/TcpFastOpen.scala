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

import java.io.File

import eu.chainfire.libsuperuser.Shell

import scala.collection.JavaConverters._
import scala.io.Source

/**
  * @author Mygod
  */
object TcpFastOpen {
  /**
    * Is kernel version >= 3.7.1.
    */
  lazy val supported: Boolean = "^(\\d+)\\.(\\d+)\\.(\\d+)".r.findFirstMatchIn(System.getProperty("os.version")) match {
    case Some(m) =>
      val kernel = m.group(1).toInt
      if (kernel < 3) false else if (kernel > 3) true else {
        val major = m.group(2).toInt
        if (major < 7) false else if (major > 7) true else m.group(3).toInt >= 1
      }
    case _ => false
  }

  def sendEnabled: Boolean = {
    val file = new File("/proc/sys/net/ipv4/tcp_fastopen")
    file.canRead && (Source.fromFile(file).mkString.trim.toInt & 1) > 0
  }

  def enabled(value: Boolean): String = if (sendEnabled != value) {
    val res = Shell.run("su", Array(
      "if echo " + (if (value) 3 else 0) + " > /proc/sys/net/ipv4/tcp_fastopen; then",
      "  echo Success.",
      "else",
      "  echo Failed.",
      "fi"), null, true)
    if (res != null) res.asScala.mkString("\n") else null
  } else null
}
