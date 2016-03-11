package com.github.shadowsocks.utils

import java.io.{BufferedReader, FileInputStream, InputStreamReader}

import android.content.Context
import com.github.shadowsocks.utils.CloseUtils._

/**
  * @author chentaov5@gmail.com
  */
object ProcessUtils {
  def getCurrentProcessName = autoClose(new FileInputStream("/proc/self/cmdline")) { inputStream =>
    autoClose(new BufferedReader(new InputStreamReader(inputStream))) { reader =>
      val sb = new StringBuilder
      var c = 0
      while ({c = reader.read(); c > 0}) sb.append(c.asInstanceOf[Char])
      sb.toString()
    }
  }

  def inShadowsocks(context: Context)(fun: => Unit) {
    if (context != null && getCurrentProcessName.equals(context.getPackageName)) fun
  }


  def inVpn(context: Context)(fun: => Unit) {
    if (context != null && getCurrentProcessName.equals(context.getPackageName + ":vpn")) fun
  }


  def inNat(context: Context)(fun: => Unit) {
    if (context != null && getCurrentProcessName.equals(context.getPackageName + ":nat")) fun
  }

}
