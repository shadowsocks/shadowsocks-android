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

import com.github.shadowsocks.utils.CloseUtils._
import java.io._

/**
  * @author Mygod
  */
object IOUtils {
  private final val BUFFER_SIZE = 32 * 1024

  def copy(in: InputStream, out: OutputStream) {
    val buffer = new Array[Byte](BUFFER_SIZE)
    while (true) {
      val count = in.read(buffer)
      if (count >= 0) out.write(buffer, 0, count) else return
    }
  }

  def writeString(file: File, content: String): Unit =
    autoClose(new FileWriter(file))(writer => writer.write(content))
  def writeString(file: String, content: String): Unit =
    autoClose(new FileWriter(file))(writer => writer.write(content))

  @throws[FileNotFoundException]
  def deleteRecursively(file: File): Unit = if (file.exists()) {
    if (file.isDirectory) file.listFiles().foreach(deleteRecursively)
    if (!file.delete()) throw new FileNotFoundException("Failed to delete: " + file.getAbsolutePath)
  }
}
