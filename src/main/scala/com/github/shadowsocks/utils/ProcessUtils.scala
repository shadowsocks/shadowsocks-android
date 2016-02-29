package com.github.shadowsocks.utils

import java.io.{BufferedReader, FileInputStream, InputStreamReader}

import android.content.Context
import com.github.shadowsocks.utils.IOUtils._

/**
  * @author chentaov5@gmail.com
  */
object ProcessUtils {

  def getCurrentProcessName: String = {
    inSafe {
      val inputStream = new FileInputStream("/proc/self/cmdline")
      val reader = new BufferedReader(new InputStreamReader(inputStream))
      autoClose(reader) {
        val sb = new StringBuilder
        var c: Int = 0
        while({c = reader.read(); c > 0})
          sb.append(c.asInstanceOf[Char])
        sb.toString()
      }
    } match {
      case Some(name: String) => name
      case None => ""
    }
  }

  def inShadowsocks[A](context: Context)(fun: => A): Unit = {
    if (context != null && getCurrentProcessName.equals(context.getPackageName)) fun
  }


  def inVpn[A](context: Context)(fun: => A): Unit = {
    if (context != null && getCurrentProcessName.equals(context.getPackageName + ":vpn")) fun
  }


  def inNat[A](context: Context)(fun: => A): Unit = {
    if (context != null && getCurrentProcessName.equals(context.getPackageName + ":nat")) fun
  }

}
