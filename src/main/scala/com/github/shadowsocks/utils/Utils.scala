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

import java.io._
import java.net._
import java.security.MessageDigest

import android.animation.{AnimatorListenerAdapter, Animator}
import android.app.ActivityManager
import android.content.pm.{ApplicationInfo, PackageManager}
import android.content.{Context, Intent}
import android.graphics._
import android.graphics.drawable.{BitmapDrawable, Drawable}
import android.os.Build
import android.provider.Settings
import android.support.v4.content.ContextCompat
import android.util.{DisplayMetrics, Base64, Log}
import android.view.View.MeasureSpec
import android.view.{Gravity, View, Window}
import android.widget.Toast
import com.github.shadowsocks.{ShadowsocksRunnerService, ShadowsocksApplication, BuildConfig}
import org.xbill.DNS._


object Utils {

  val TAG: String = "Shadowsocks"
  val DEFAULT_IPTABLES: String = "/system/bin/iptables"
  val ALTERNATIVE_IPTABLES: String = "iptables"
  var initialized: Boolean = false
  var hasRedirectSupport: Int = -1
  var iptables: String = null

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
  def dpToPx(context: Context, dp: Float): Float = {
    val density = context.getResources.getDisplayMetrics.density
    dp * density
  }

  def pxToDp(context: Context, px: Float): Float = {
    val density = context.getResources.getDisplayMetrics.density
    px / density
  }

  def getBitmap(text: String, width: Int, height: Int, background: Int): Bitmap = {
    val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    val size = bitmap.getHeight / 4
    val canvas = new Canvas(bitmap)
    canvas.drawColor(background)
    val paint = new Paint()
    paint.setColor(Color.WHITE)
    paint.setTextSize(size)
    val bounds = new Rect()
    paint.getTextBounds(text, 0, text.length, bounds)
    canvas
      .drawText(text, (bitmap.getWidth - bounds.width()) / 2,
      bitmap.getHeight - (bitmap.getHeight - bounds.height()) / 2, paint)
    bitmap
  }

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

  // Blocked > 3 seconds
  def toggleAirplaneMode(context: Context) = {
    if (Console.isRoot) {
      Console.runRootCommand("ndc resolver flushdefaultif", "ndc resolver flushif wlan0")
      true
    } else if (Build.VERSION.SDK_INT < 17) {
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

  def isServiceStarted(name: String, context: Context): Boolean = {
    import scala.collection.JavaConversions._

    val activityManager = context
      .getSystemService(Context.ACTIVITY_SERVICE)
      .asInstanceOf[ActivityManager]
    val services = activityManager.getRunningServices(Integer.MAX_VALUE)

    if (services != null) {
      for (service <- services) {
        if (service.service.getClassName == name) {
          return true
        }
      }
    }

    false
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
   * Get local IPv4 address
   */
  def getIPv4Address: Option[String] = {
    try {
      val interfaces = NetworkInterface.getNetworkInterfaces
      while (interfaces.hasMoreElements) {
        val intf = interfaces.nextElement()
        val addrs = intf.getInetAddresses
        while (addrs.hasMoreElements) {
          val addr = addrs.nextElement()
          if (!addr.isLoopbackAddress && !addr.isLinkLocalAddress) {
            if (addr.isInstanceOf[Inet4Address]) {
              return Some(addr.getHostAddress.toUpperCase)
            }
          }
        }
      }
    } catch {
      case ex: Exception =>
        Log.e(TAG, "Failed to get interfaces' addresses.", ex)
    }
    None
  }

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

  /**
   * Check the system's iptables
   * Default to use the app's iptables
   */
  def checkIptables() {

    if (!Console.isRoot) {
      iptables = DEFAULT_IPTABLES
      return
    }

    iptables = DEFAULT_IPTABLES

    var compatible: Boolean = false
    var version: Boolean = false

    val lines = Console.runRootCommand(iptables + " --version", iptables + " -L -t nat -n")
    if (lines == null) return

    if (lines.contains("OUTPUT")) {
      compatible = true
    }
    if (lines.contains("v1.4.")) {
      version = true
    }
    if (!compatible || !version) {
      iptables = ALTERNATIVE_IPTABLES
      if (!new File(iptables).exists) iptables = "iptables"
    }
  }

  def drawableToBitmap(drawable: Drawable): Bitmap = {
    drawable match {
      case d: BitmapDrawable =>
        return d.getBitmap
      case _ =>
    }

    val width = if (drawable.getIntrinsicWidth > 0) drawable.getIntrinsicWidth else 1
    val height = if (drawable.getIntrinsicWidth > 0) drawable.getIntrinsicWidth else 1

    val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    val canvas = new Canvas(bitmap)
    drawable.setBounds(0, 0, canvas.getWidth, canvas.getHeight)
    drawable.draw(canvas)

    bitmap
  }

  def getAppIcon(c: Context, packageName: String): Drawable = {
    val pm: PackageManager = c.getPackageManager
    val icon: Drawable = ContextCompat.getDrawable(c, android.R.drawable.sym_def_app_icon)
    try {
      pm.getApplicationIcon(packageName)
    } catch {
      case e: PackageManager.NameNotFoundException => icon
    }
  }

  def getAppIcon(c: Context, uid: Int): Drawable = {
    val pm: PackageManager = c.getPackageManager
    val icon: Drawable = ContextCompat.getDrawable(c, android.R.drawable.sym_def_app_icon)
    val packages: Array[String] = pm.getPackagesForUid(uid)
    if (packages != null) {
      if (packages.length >= 1) {
        try {
          val appInfo: ApplicationInfo = pm.getApplicationInfo(packages(0), 0)
          return pm.getApplicationIcon(appInfo)
        } catch {
          case e: PackageManager.NameNotFoundException =>
            Log.e(c.getPackageName, "No package found matching with the uid " + uid)
        }
      }
    } else {
      Log.e(c.getPackageName, "Package not found for uid " + uid)
    }
    icon
  }

  def getHasRedirectSupport: Boolean = {
    if (hasRedirectSupport == -1) initHasRedirectSupported()
    hasRedirectSupport == 1
  }

  def getIptables: String = {
    if (iptables == null) checkIptables()
    iptables
  }

  def initHasRedirectSupported() {
    if (!Console.isRoot) return
    hasRedirectSupport = 1

    val command = Utils.getIptables + " -t nat -A OUTPUT -p udp --dport 54 -j REDIRECT --to 8154"
    val lines = Console.runRootCommand(command, command.replace("-A", "-D"))
    if (lines == null) return
    if (lines.contains("No chain/target/match")) {
      hasRedirectSupport = 0
    }
  }

  def isInitialized: Boolean = {
    initialized match {
      case true => true
      case _ =>
        initialized = true
        false
    }
  }

  def startSsService(context: Context): Unit = {
    val isInstalled: Boolean = ShadowsocksApplication.settings.getBoolean(ShadowsocksApplication.getVersionName, false)
    if (!isInstalled) return

    val intent = new Intent(context, classOf[ShadowsocksRunnerService])
    context.startService(intent)
  }

  def stopSsService(context: Context): Unit = {
    context.sendBroadcast(new Intent(Action.CLOSE))
  }
}


