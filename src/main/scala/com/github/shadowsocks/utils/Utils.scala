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

import java.net._
import java.security.MessageDigest

import android.animation.{Animator, AnimatorListenerAdapter}
import android.content.pm.PackageManager
import android.content.{Context, Intent}
import android.graphics._
import android.os.Build
import android.provider.Settings
import android.util.{Base64, DisplayMetrics, Log}
import android.view.View.MeasureSpec
import android.view.{Gravity, View, Window}
import android.widget.Toast
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.{BuildConfig, ShadowsocksRunnerService}
import eu.chainfire.libsuperuser.Shell
import org.xbill.DNS._

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.util.{Failure, Try}

object Utils {
  private val TAG = "Shadowsocks"

  def isLollipopOrAbove: Boolean = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP

  def getSignature(context: Context): String = {
    val info = context
      .getPackageManager
      .getPackageInfo(context.getPackageName, PackageManager.GET_SIGNATURES)
    val mdg = MessageDigest.getInstance("SHA-1")
    mdg.update(info.signatures(0).toByteArray)
    new String(Base64.encode(mdg.digest, 0))
  }

  def dpToPx(context: Context, dp: Int): Int =
    Math.round(dp * (context.getResources.getDisplayMetrics.xdpi / DisplayMetrics.DENSITY_DEFAULT))

  /*
     * round or floor depending on whether you are using offsets(floor) or
     * widths(round)
     */

  // Based on: http://stackoverflow.com/a/21026866/2245107
  def positionToast(toast: Toast, view: View, window: Window, offsetX: Int = 0, offsetY: Int = 0) = {
    val rect = new Rect
    window.getDecorView.getWindowVisibleDisplayFrame(rect)
    val viewLocation = new Array[Int](2)
    view.getLocationInWindow(viewLocation)
    val metrics = new DisplayMetrics
    window.getWindowManager.getDefaultDisplay.getMetrics(metrics)
    val toastView = toast.getView
    toastView.measure(MeasureSpec.makeMeasureSpec(metrics.widthPixels, MeasureSpec.UNSPECIFIED),
      MeasureSpec.makeMeasureSpec(metrics.heightPixels, MeasureSpec.UNSPECIFIED))
    toast.setGravity(Gravity.LEFT | Gravity.TOP,
      viewLocation(0) - rect.left + (view.getWidth - toast.getView.getMeasuredWidth) / 2 + offsetX,
      viewLocation(1) - rect.top + view.getHeight + offsetY)
    toast
  }

  def crossFade(context: Context, from: View, to: View) {
    def shortAnimTime = context.getResources.getInteger(android.R.integer.config_shortAnimTime)
    to.setAlpha(0)
    to.setVisibility(View.VISIBLE)
    to.animate().alpha(1).setDuration(shortAnimTime)
    from.animate().alpha(0).setDuration(shortAnimTime).setListener(new AnimatorListenerAdapter {
      override def onAnimationEnd(animation: Animator) = from.setVisibility(View.GONE)
    })
  }

  def printToFile(f: java.io.File)(op: java.io.PrintWriter => Unit) {
    val p = new java.io.PrintWriter(f)
    try {
      op(p)
    } finally {
      p.close()
    }
  }

  // Because /sys/class/net/* isn't accessible since API level 24
  final val FLUSH_DNS = "for if in /sys/class/net/*; do " +
    "if [ \"down\" != $(cat $if/operstate) ]; then " +  // up or unknown
      "ndc resolver flushif ${if##*/}; " +
    "fi " +
  "done; echo done"

  // Blocked > 3 seconds
  def toggleAirplaneMode(context: Context) = {
    val result = Shell.SU.run(FLUSH_DNS)
    if (result != null && !result.isEmpty) true else if (Build.VERSION.SDK_INT < 17) {
      toggleBelowApiLevel17(context)
      true
    } else false
  }

  //noinspection ScalaDeprecation
  private def toggleBelowApiLevel17(context: Context) {
    // Android 4.2 below
    Settings.System.putInt(context.getContentResolver, Settings.System.AIRPLANE_MODE_ON, 1)
    val enableIntent = new Intent(Intent.ACTION_AIRPLANE_MODE_CHANGED)
    enableIntent.putExtra("state", true)
    context.sendBroadcast(enableIntent)
    Thread.sleep(3000)

    Settings.System.putInt(context.getContentResolver, Settings.System.AIRPLANE_MODE_ON, 0)
    val disableIntent = new Intent(Intent.ACTION_AIRPLANE_MODE_CHANGED)
    disableIntent.putExtra("state", false)
    context.sendBroadcast(disableIntent)
  }

  def resolve(host: String, addrType: Int): Option[String] = {
    try {
      val lookup = new Lookup(host, addrType)
      val resolver = new SimpleResolver("114.114.114.114")
      resolver.setTimeout(5)
      lookup.setResolver(resolver)
      val result = lookup.run()
      if (result == null) return None
      val records = scala.util.Random.shuffle(result.toList)
      for (r <- records) {
        addrType match {
          case Type.A =>
            return Some(r.asInstanceOf[ARecord].getAddress.getHostAddress)
          case Type.AAAA =>
            return Some(r.asInstanceOf[AAAARecord].getAddress.getHostAddress)
        }
      }
    } catch {
      case e: Exception =>
    }
    None
  }

  def resolve(host: String): Option[String] = {
    try {
      val addr = InetAddress.getByName(host)
      Some(addr.getHostAddress)
    } catch {
      case e: UnknownHostException => None
    }
  }

  def resolve(host: String, enableIPv6: Boolean): Option[String] = {
    if (enableIPv6 && Utils.isIPv6Support) {
      resolve(host, Type.AAAA) match {
        case Some(addr) =>
          return Some(addr)
        case None =>
      }
    }
    resolve(host, Type.A) match {
      case Some(addr) =>
        return Some(addr)
      case None =>
    }
    resolve(host) match {
      case Some(addr) =>
        return Some(addr)
      case None =>
    }
    None
  }

  private lazy val isNumericMethod = classOf[InetAddress].getMethod("isNumeric", classOf[String])
  def isNumeric(address: String): Boolean = isNumericMethod.invoke(null, address).asInstanceOf[Boolean]

  /**
   * If there exists a valid IPv6 interface
   */
  def isIPv6Support: Boolean = {
    try {
      val interfaces = NetworkInterface.getNetworkInterfaces
      while (interfaces.hasMoreElements) {
        val intf = interfaces.nextElement()
        val addrs = intf.getInetAddresses
        while (addrs.hasMoreElements) {
          val addr = addrs.nextElement()
          if (!addr.isLoopbackAddress && !addr.isLinkLocalAddress) {
            if (addr.isInstanceOf[Inet6Address]) {
              if (BuildConfig.DEBUG) Log.d(TAG, "IPv6 address detected")
              return true
            }
          }
        }
      }
    } catch {
      case ex: Exception =>
        Log.e(TAG, "Failed to get interfaces' addresses.", ex)
    }
    false
  }

  def startSsService(context: Context) {
    val isInstalled: Boolean = app.settings.getBoolean(app.getVersionName, false)
    if (!isInstalled) return

    val intent = new Intent(context, classOf[ShadowsocksRunnerService])
    context.startService(intent)
  }

  def stopSsService(context: Context) {
    val intent = new Intent(Action.CLOSE)
    context.sendBroadcast(intent)
  }

  private val handleFailure: Try[_] => Unit = {
    case Failure(e) => e.printStackTrace()
    case _ =>
  }

  def ThrowableFuture[T](f: => T) = Future(f) onComplete handleFailure
}
