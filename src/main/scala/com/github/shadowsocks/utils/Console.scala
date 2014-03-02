package com.github.shadowsocks.utils

import eu.chainfire.libsuperuser.Shell
import eu.chainfire.libsuperuser.Shell.{OnCommandResultListener, SU, Builder}
import java.util
import android.util.Log

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
          if (exitCode < 0) shell = null
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
    shell.addCommand(commands)
    shell.waitForIdle()
  }

  def runRootCommand(command: String): String = runRootCommand(Array(command))

  def runRootCommand(commands: Array[String]): String = {
    if (!isRoot) {
      return null
    }
    if (rootShell == null) {
      openRootShell()
    }
    val sb = new StringBuilder
    rootShell.addCommand(commands, 0, new OnCommandResultListener {
      override def onCommandResult(commandCode: Int, exitCode: Int,
        output: util.List[String]) {
        if (exitCode < 0) {
          rootShell = null
        } else {
          import scala.collection.JavaConversions._
          output.foreach(line => sb.append(line).append('\n'))
        }
      }
    })
    if (rootShell.waitForIdle()) sb.toString()
    else null
  }

  def isRoot: Boolean = SU.available()
}
