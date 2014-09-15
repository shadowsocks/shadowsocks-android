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

import eu.chainfire.libsuperuser.Shell
import eu.chainfire.libsuperuser.Shell.{OnCommandResultListener, SU, Builder}
import java.util

object Console {

  private var shell: Shell.Interactive = null
  private var rootShell: Shell.Interactive = null

  private def openShell() {
    if (shell == null) {
      val builder = new Builder()
      shell = builder
        .useSH()
        .setWatchdogTimeout(10)
        .open(new OnCommandResultListener {
        override def onCommandResult(commandCode: Int, exitCode: Int,
          output: util.List[String]) {
          if (exitCode < 0) {
            shell.close()
            shell = null
          }
        }
      })
    }
  }

  private def openRootShell() {
    if (rootShell == null) {
      val builder = new Builder()
      rootShell = builder
        .setShell(SU.shell(0, "u:r:untrusted_app:s0"))
        .setWantSTDERR(true)
        .setWatchdogTimeout(10)
        .open()
    }
  }

  def runCommand(command: String) {
    runCommand(Array(command))
  }

  def runCommand(commands: Array[String]) {
    if (shell == null) {
      openShell()
    }
    val ts = shell
    ts.addCommand(commands)
    ts.waitForIdle()
  }

  def runRootCommand(command: String): String = runRootCommand(Array(command))

  def runRootCommand(commands: Array[String]): String = {
    if (!isRoot) {
      return null
    }
    if (rootShell == null) {
      openRootShell()
    }
    val ts = rootShell
    val sb = new StringBuilder
    ts.addCommand(commands, 0, new OnCommandResultListener {
      override def onCommandResult(commandCode: Int, exitCode: Int,
        output: util.List[String]) {
        if (exitCode < 0) {
          rootShell.close()
          rootShell = null
        } else {
          import scala.collection.JavaConversions._
          output.foreach(line => sb.append(line).append('\n'))
        }
      }
    })
    if (ts.waitForIdle()) sb.toString()
    else null
  }

  def isRoot: Boolean = SU.available()
}
