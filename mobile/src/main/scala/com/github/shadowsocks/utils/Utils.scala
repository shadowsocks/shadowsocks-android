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

import java.lang.reflect.InvocationTargetException
import java.net._
import java.security.MessageDigest

import android.content.pm.PackageManager
import android.content.{Context, Intent}
import android.graphics._
import android.os.Build
import android.util.{Base64, DisplayMetrics, Log}
import android.view.View.MeasureSpec
import android.view.{Gravity, View, Window}
import android.widget.Toast
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.bg.{ProxyService, TransproxyService, VpnService}
import com.github.shadowsocks.BuildConfig
import org.xbill.DNS._

import scala.collection.JavaConversions._
import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.util.{Failure, Try}

object Utils {
  private val TAG = "Shadowsocks"

  def isLollipopOrAbove: Boolean = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP

  def getSignature(context: Context): String = {
    val mdg = MessageDigest.getInstance("SHA-1")
    mdg.update(app.info.signatures(0).toByteArray)
    new String(Base64.encode(mdg.digest, 0))
  }

  /*
     * round or floor depending on whether you are using offsets(floor) or
     * widths(round)
     */

  // Based on: http://stackoverflow.com/a/21026866/2245107
  def positionToast(toast: Toast, view: View, window: Window, offsetX: Int = 0, offsetY: Int = 0): Toast = {
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

  def resolve(host: String, addrType: Int): Option[String] = {
    try {
      val lookup = new Lookup(host, addrType)
      val resolver = new SimpleResolver("208.67.220.220")
      resolver.setTCP(true)
      resolver.setPort(443)
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
      case _: Exception =>
    }
    None
  }

  def resolve(host: String): Option[String] = try Some(InetAddress.getByName(host).getHostAddress) catch {
    case _: UnknownHostException => None
  }

  def resolve(host: String, enableIPv6: Boolean): Option[String] =
    (if (enableIPv6 && isIPv6Support) resolve(host, Type.AAAA) else None).orElse(resolve(host, Type.A))
      .orElse(resolve(host))

  private lazy val isNumericMethod = classOf[InetAddress].getMethod("isNumeric", classOf[String])
  private lazy val parseNumericAddressMethod = classOf[InetAddress].getMethod("parseNumericAddress", classOf[String])
  def isNumeric(address: String): Boolean = isNumericMethod.invoke(null, address).asInstanceOf[Boolean]
  def parseNumericAddress(address: String): InetAddress =
    try parseNumericAddressMethod.invoke(null, address).asInstanceOf[InetAddress] catch {
      case exc: InvocationTargetException => throw exc.getCause match {
        case null => exc
        case cause => cause
      }
    }

  /**
   * If there exists a valid IPv6 interface
   */
  def isIPv6Support: Boolean = try {
    val result = NetworkInterface.getNetworkInterfaces.flatMap(_.getInetAddresses)
      .count(addr => addr.isInstanceOf[Inet6Address] && !addr.isLoopbackAddress && !addr.isLinkLocalAddress) > 0
    if (result && BuildConfig.DEBUG) Log.d(TAG, "IPv6 address detected")
    result
  } catch {
    case ex: Exception =>
      Log.e(TAG, "Failed to get interfaces' addresses.", ex)
      app.track(ex)
      false
  }

  def startSsService(context: Context) {
    val intent = new Intent(context, app.serviceClass)
    if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(intent) else context.startService(intent)
  }
  def reloadSsService(context: Context): Unit = context.sendBroadcast(new Intent(Action.RELOAD))
  def stopSsService(context: Context) {
    val intent = new Intent(Action.CLOSE)
    context.sendBroadcast(intent)
  }

  private val handleFailure: Try[_] => Unit = {
    case Failure(e) =>
      e.printStackTrace()
      app.track(e)
    case _ =>
  }

  def ThrowableFuture[T](f: => T): Unit = Future(f) onComplete handleFailure

  def parsePort(str: String, default: Int, min: Int = 1025): Int = (try str.toInt catch {
    case _: NumberFormatException => default
  }) match {
    case x if x < min => default
    case x if x > 65535 => default
    case x => x
  }
}
