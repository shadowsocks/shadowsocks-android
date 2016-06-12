/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2014 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks.utils

import java.util

import eu.chainfire.libsuperuser.Shell
import eu.chainfire.libsuperuser.Shell.{Builder, SU}

object Console {

  private def openShell(): Shell.Interactive = {
    val builder = new Builder()
    builder
      .useSH()
      .setWatchdogTimeout(10)
      .open()
  }

  private def openRootShell(context: String): Shell.Interactive = {
    val builder = new Builder()
    builder
      .setShell(SU.shell(0, context))
      .setWantSTDERR(true)
      .setWatchdogTimeout(10)
      .open()
  }

  def runCommand(commands: Array[String]) {
    val shell = openShell()
    shell.addCommand(commands, 0, ((commandCode: Int, exitCode: Int, output: util.List[String]) =>
      if (exitCode < 0) shell.close()): Shell.OnCommandResultListener)
    shell.waitForIdle()
    shell.close()
  }

  def runRootCommand(commands: String*): String = runRootCommand(commands.toArray)
  def runRootCommand(commands: Array[String]): String = {
    val shell = openRootShell("u:r:init_shell:s0")
    val sb = new StringBuilder
    shell.addCommand(commands, 0, ((_, exitCode, output) => {
      if (exitCode < 0) {
        shell.close()
      } else {
        import scala.collection.JavaConversions._
        output.foreach(line => sb.append(line).append('\n'))
      }
    }): Shell.OnCommandResultListener)
    if (shell.waitForIdle()) {
      shell.close()
      sb.toString()
    }
    else {
      shell.close()
      null
    }
  }

  def isRoot: Boolean = SU.available()
}
