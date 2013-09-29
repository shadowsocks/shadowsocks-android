/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
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
package com.github.shadowsocks

import android.content.{Intent, SharedPreferences, Context}
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.graphics.drawable.{BitmapDrawable, Drawable}
import android.util.Log
import java.io._
import java.net.{UnknownHostException, InetAddress, NetworkInterface}
import org.apache.http.conn.util.InetAddressUtils
import scala.collection.mutable.ArrayBuffer
import org.xbill.DNS._
import scala.Some
import android.graphics._
import android.app.ActivityManager
import android.os.Build
import android.provider.Settings
import scala.Some

object Config {
  val SHADOWSOCKS = "{\"server\": [%s], \"server_port\": %d, \"local_port\": %d, \"password\": %s, \"timeout\": %d}"
  val REDSOCKS = "base {" +
    " log_debug = off;" +
    " log_info = off;" +
    " log = stderr;" +
    " daemon = on;" +
    " redirector = iptables;" +
    "}" +
    "redsocks {" +
    " local_ip = 127.0.0.1;" +
    " local_port = 8123;" +
    " ip = 127.0.0.1;" +
    " port = %d;" +
    " type = socks5;" +
    "}"
  val PDNSD =
    """
      |global {
      | perm_cache = 2048;
      | cache_dir = "/data/data/com.github.shadowsocks";
      | server_ip = %s;
      | server_port = 8153;
      | query_method = tcp_only;
      | run_ipv4 = on;
      | min_ttl = 15m;
      | max_ttl = 1w;
      | timeout = 10;
      | daemon = on;
      | pid_file = "/data/data/com.github.shadowsocks/pdnsd.pid";
      |}
      |
      |server {
      | label = "root-servers";
      | ip = 8.8.8.8, 8.8.4.4, 208.67.222.222, 208.67.220.220;
      | timeout = 5;
      |}
      |
      |rr {
      | name=localhost;
      | reverse=on;
      | a=127.0.0.1;
      | owner=localhost;
      | soa=localhost,root.localhost,42,86400,900,86400,86400;
      |}
    """.stripMargin

  def printToFile(f: java.io.File)(op: java.io.PrintWriter => Unit) {
    val p = new java.io.PrintWriter(f)
    try {
      op(p)
    } finally {
      p.close()
    }
  }
}

case class Config(isGlobalProxy: Boolean, isGFWList: Boolean, isBypassApps: Boolean, isTrafficStat: Boolean,
                  profileName: String, var proxy: String, sitekey: String, encMethod: String, remotePort: Int,
                  localPort: Int, proxiedAppString: String)

object Key {
  val profileId = "profileId"
  val profileName = "profileName"

  val proxied = "Proxyed"

  val isRoot = "isRoot"
  val status = "status"
  val proxyedApps = "proxyedApps"

  val isRunning = "isRunning"
  val isAutoConnect = "isAutoConnect"

  val isGlobalProxy = "isGlobalProxy"
  val isGFWList = "isGFWList"
  val isBypassApps = "isBypassApps"
  val isTrafficStat = "isTrafficStat"

  val proxy = "proxy"
  val sitekey = "sitekey"
  val encMethod = "encMethod"
  val remotePort = "remotePort"
  val localPort = "port"
}

object Scheme {
  val APP = "app://"
  val PROFILE = "profile://"
}

object State {
  val INIT = 0
  val CONNECTING = 1
  val CONNECTED = 2
  val STOPPED = 3
}

object Action {
  val CLOSE = "com.github.shadowsocks.ACTION_SHUTDOWN"
  val UPDATE_STATE = "com.github.shadowsocks.ACTION_UPDATE_STATE"
  val UPDATE_FRAGMENT = "com.github.shadowsocks.ACTION_UPDATE_FRAGMENT"
}

object Extra {
  val STATE = "state"
  val MESSAGE = "message"

  def save(settings: SharedPreferences, config: Config) {
    val edit = settings.edit()

    edit.putBoolean(Key.isGlobalProxy, config.isGlobalProxy)
    edit.putBoolean(Key.isGFWList, config.isGFWList)
    edit.putBoolean(Key.isBypassApps, config.isBypassApps)
    edit.putBoolean(Key.isTrafficStat, config.isTrafficStat)

    edit.putString(Key.profileName, config.profileName)
    edit.putString(Key.proxy, config.proxy)
    edit.putString(Key.sitekey, config.sitekey)
    edit.putString(Key.encMethod, config.encMethod)
    edit.putString(Key.remotePort, config.remotePort.toString)
    edit.putString(Key.localPort, config.localPort.toString)

    edit.apply()
  }

  def get(intent: Intent): Config = {
    val isGlobalProxy = intent.getBooleanExtra(Key.isGlobalProxy, false)
    val isGFWList = intent.getBooleanExtra(Key.isGFWList, false)
    val isBypassApps = intent.getBooleanExtra(Key.isBypassApps, false)
    val isTrafficStat = intent.getBooleanExtra(Key.isTrafficStat, false)

    val profileName = intent.getStringExtra(Key.profileName)
    val proxy = intent.getStringExtra(Key.proxy)
    val sitekey = intent.getStringExtra(Key.sitekey)
    val encMethod = intent.getStringExtra(Key.encMethod)
    val remotePort = intent.getIntExtra(Key.remotePort, 1984)
    val localPort = intent.getIntExtra(Key.localPort, 1984)
    val proxiedString = intent.getStringExtra(Key.proxied)

    new Config(isGlobalProxy, isGFWList, isBypassApps, isTrafficStat, profileName, proxy, sitekey, encMethod, remotePort,
      localPort, proxiedString)
  }

  def put(settings: SharedPreferences, intent: Intent) {
    val isGlobalProxy = settings.getBoolean(Key.isGlobalProxy, false)
    val isGFWList = settings.getBoolean(Key.isGFWList, false)
    val isBypassApps = settings.getBoolean(Key.isBypassApps, false)
    val isTrafficStat = settings.getBoolean(Key.isTrafficStat, false)

    val profileName = settings.getString(Key.profileName, "default")
    val proxy = settings.getString(Key.proxy, "127.0.0.1") match {
      case "198.199.101.152" => BuildConfig.SERVER
      case s: String => s
      case _ => "127.0.0.1"
    }
    val sitekey = settings.getString(Key.sitekey, "default")
    val encMethod =  {
      if (proxy == BuildConfig.SERVER) {
        BuildConfig.METHOD
      } else {
        settings.getString(Key.encMethod, "table")
      }
    }
    val remotePort: Int = try {
      if (proxy == BuildConfig.SERVER) {
        scala.util.Random.shuffle(BuildConfig.PORTS.toSeq).toSeq(0)
      } else {
        Integer.valueOf(settings.getString(Key.remotePort, "1984"))
      }
    } catch {
      case ex: NumberFormatException => {
        1984
      }
    }
    val localProt: Int = try {
      Integer.valueOf(settings.getString(Key.localPort, "1984"))
    } catch {
      case ex: NumberFormatException => {
        1984
      }
    }
    val proxiedAppString = settings.getString(Key.proxied, "")

    intent.putExtra(Key.isGlobalProxy, isGlobalProxy)
    intent.putExtra(Key.isGFWList, isGFWList)
    intent.putExtra(Key.isBypassApps, isBypassApps)
    intent.putExtra(Key.isTrafficStat, isTrafficStat)

    intent.putExtra(Key.profileName, profileName)
    intent.putExtra(Key.proxy, proxy)
    intent.putExtra(Key.sitekey, sitekey)
    intent.putExtra(Key.encMethod, encMethod)
    intent.putExtra(Key.remotePort, remotePort)
    intent.putExtra(Key.localPort, localProt)

    intent.putExtra(Key.proxied, proxiedAppString)
  }
}

object Utils {

  val TAG: String = "Shadowsocks"
  val ABI_PROP: String = "ro.product.cpu.abi"
  val ABI2_PROP: String = "ro.product.cpu.abi2"
  val ARM_ABI: String = "arm"
  val X86_ABI: String = "x86"
  val MIPS_ABI: String = "mips"
  val DEFAULT_SHELL: String = "/system/bin/sh"
  val DEFAULT_ROOT: String = "/system/bin/su"
  val ALTERNATIVE_ROOT: String = "/system/xbin/su"
  val DEFAULT_IPTABLES: String = "/data/data/com.github.shadowsocks/iptables"
  val ALTERNATIVE_IPTABLES: String = "/system/bin/iptables"
  val TIME_OUT: Int = -99
  var initialized: Boolean = false
  var hasRedirectSupport: Int = -1
  var isRoot: Int = -1
  var shell: String = null
  var root_shell: String = null
  var iptables: String = null
  var data_path: String = null
  var rootTries = 0

  /*
     * round or floor depending on whether you are using offsets(floor) or
     * widths(round)
     */
  def dpToPx (context: Context, dp: Float): Float = {
    val density = context.getResources.getDisplayMetrics.density
    dp * density
  }

  def pxToDp (context: Context, px: Float): Float = {
    val density = context.getResources.getDisplayMetrics.density
    px / density
  }

  def getBitmap(text: String, width: Int, height: Int, background: Int): Bitmap = {
    val bitmap = Bitmap.createBitmap(width,
      height, Bitmap.Config.ARGB_8888)
    val size = bitmap.getHeight / 4
    val canvas = new Canvas(bitmap)
    canvas.drawColor(background)
    val paint = new Paint()
    paint.setColor(Color.WHITE)
    paint.setTextSize(size)
    val bounds = new Rect()
    paint.getTextBounds(text, 0, text.length, bounds)
    canvas.drawText(text, (bitmap.getWidth - bounds.width()) / 2,
      bitmap.getHeight - (bitmap.getHeight - bounds.height()) / 2, paint)
    bitmap
  }

  // Blocked > 3 seconds
  def toggleAirplaneMode(context: Context) {
    if (Build.VERSION.SDK_INT >= 17) {
      toggleAboveApiLevel17()
    } else {
      toggleBelowApiLevel17(context)
    }
  }

  private def toggleAboveApiLevel17() {
    // Android 4.2 and above
    Utils.runRootCommand("settings put global airplane_mode_on 1\n"
      + "am broadcast -a android.intent.action.AIRPLANE_MODE --ez state true\n"
      + "settings put global airplane_mode_on 0\n"
      + "am broadcast -a android.intent.action.AIRPLANE_MODE --ez state false\n"
    )
  }

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
        case Some(addr) => {
          return Some(addr)
        }
        case None =>
      }
    }
    resolve(host, Type.A) match {
      case Some(addr) => {
        return Some(addr)
      }
      case None =>
    }
    resolve(host) match {
      case Some(addr) => {
        return Some(addr)
      }
      case None =>
    }
    None
  }

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
            val sAddr = addr.getHostAddress.toUpperCase
            if (InetAddressUtils.isIPv4Address(sAddr)) {
              return Some(sAddr)
            }
          }
        }
      }
    } catch {
      case ex: Exception => {
        Log.e(TAG, "Failed to get interfaces' addresses.", ex)
      }
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
            val sAddr = addr.getHostAddress.toUpperCase
            if (InetAddressUtils.isIPv6Address(sAddr)) {
              if (BuildConfig.DEBUG) Log.d(TAG, "IPv6 address detected")
              return true
            }
          }
        }
      }
    } catch {
      case ex: Exception => {
        Log.e(TAG, "Failed to get interfaces' addresses.", ex)
      }
    }
    false
  }

  /**
   * Get the ABI of the device
   * @return The ABI of the device, or ARM_ABI if not found
   */
  def getABI: String = {
    val prop = getSystemProperty(ABI_PROP)
    prop match {
      case abi if abi.toLowerCase.contains(ARM_ABI) => ARM_ABI
      case abi if abi.toLowerCase.contains(X86_ABI) => X86_ABI
      case _ => ARM_ABI
    }
  }

  /**
   * Returns a SystemProperty
   *
   * @param propName The Property to retrieve
   * @return The Property, or NULL if not found
   */
  def getSystemProperty(propName: String): String = {
    val p: Process = Runtime.getRuntime.exec("getprop " + propName)
    val lines = scala.io.Source.fromInputStream(p.getInputStream).getLines()
    if (lines.hasNext) lines.next() else null
  }

  /**
   * Check the system's iptables
   * Default to use the app's iptables
   */
  def checkIptables() {

    if (!Utils.getRoot) {
      iptables = DEFAULT_IPTABLES
      return
    }

    iptables = DEFAULT_IPTABLES

    var lines: String = null
    var compatible: Boolean = false
    var version: Boolean = false

    val sb = new StringBuilder
    val command = iptables + " --version\n" + iptables + " -L -t nat -n\n" + "exit\n"
    val exitcode = runScript(command, sb, 10 * 1000, asroot = true)
    if (exitcode == TIME_OUT) return

    lines = sb.toString()
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

  def getAppIcon(c: Context, uid: Int): Drawable = {
    val pm: PackageManager = c.getPackageManager
    val icon: Drawable = c.getResources.getDrawable(android.R.drawable.sym_def_app_icon)
    val packages: Array[String] = pm.getPackagesForUid(uid)
    if (packages != null) {
      if (packages.length >= 1) {
        try {
          val appInfo: ApplicationInfo = pm.getApplicationInfo(packages(0), 0)
          return pm.getApplicationIcon(appInfo)
        } catch {
          case e: PackageManager.NameNotFoundException => {
            Log.e(c.getPackageName, "No package found matching with the uid " + uid)
          }
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

  def getShell: String = {
    if (shell == null) {
      shell = DEFAULT_SHELL
      if (!new File(shell).exists) shell = "sh"
    }
    shell
  }

  def initHasRedirectSupported() {
    if (!Utils.getRoot) return
    hasRedirectSupport = 1

    val sb = new StringBuilder
    val command = Utils.getIptables + " -t nat -A OUTPUT -p udp --dport 54 -j REDIRECT --to 8154"
    val exitcode: Int = runScript(command, sb, 10 * 1000, asroot = true)
    val lines = sb.toString()

    Utils.runRootCommand(command.replace("-A", "-D"))
    if (exitcode == TIME_OUT) return
    if (lines.contains("No chain/target/match")) {
      hasRedirectSupport = 0
    }
  }

  def isInitialized: Boolean = {
    initialized match {
      case true => true
      case _ => {
        initialized = true
        false
      }
    }
  }

  def getRoot: Boolean = {
    if (isRoot != -1) return isRoot == 1
    if (new File(DEFAULT_ROOT).exists) {
      root_shell = DEFAULT_ROOT
    } else if (new File(ALTERNATIVE_ROOT).exists) {
      root_shell = ALTERNATIVE_ROOT
    } else {
      root_shell = "su"
    }
    val sb = new StringBuilder
    val command: String = "id\n"
    val exitcode: Int = runScript(command, sb, 10 * 1000, asroot = true)
    if (exitcode == TIME_OUT) {
      return false
    }
    val lines = sb.toString()
    if (lines.contains("uid=0")) {
      isRoot = 1
    } else {
      if (rootTries >= 1) isRoot = 0
      rootTries += 1
    }
    isRoot == 1
  }

  def runCommand(command: String): Boolean = {
    runCommand(command, 10 * 1000)
  }

  def runCommand(command: String, timeout: Int): Boolean = {
    if (BuildConfig.DEBUG) Log.d(TAG, command)
    runScript(command, null, timeout, asroot = false)
    true
  }

  def runRootCommand(command: String): Boolean = {
    runRootCommand(command, 10 * 1000)
  }

  def runRootCommand(command: String, timeout: Int): Boolean = {
    if (!Utils.getRoot) {
      Log.e(TAG, "Cannot get ROOT permission: " + root_shell)
      return false
    }
    if (BuildConfig.DEBUG) Log.d(TAG, command)
    runScript(command, null, timeout, asroot = true)
    true
  }

  def runScript(script: String, result: StringBuilder, timeout: Long, asroot: Boolean): Int = {
    val runner: Utils.ScriptRunner = new Utils.ScriptRunner(script, result, asroot)
    runner.start()
    try {
      if (timeout > 0) {
        runner.join(timeout)
      } else {
        runner.join()
      }
      if (runner.isAlive) {
        runner.destroy()
        runner.join(1000)
        return TIME_OUT
      }
    } catch {
      case ex: InterruptedException => {
        return TIME_OUT
      }
    }
    runner.exitcode
  }

  /**
   * Internal thread used to execute scripts (as root or not).
   * Creates a new script runner.
   *
   * @param scripts script to run
   * @param result result output
   * @param asroot if true, executes the script as root
   */
  class ScriptRunner(val scripts: String, val result: StringBuilder, val asroot: Boolean)
    extends Thread {

    var exitcode: Int = -1
    val pid: Array[Int] = new Array[Int](1)
    var pipe: FileDescriptor = null

    override def destroy() {
      if (pid(0) != -1) {
        Exec.hangupProcessGroup(pid(0))
        pid(0) = -1
      }
      if (pipe != null) {
        Exec.close(pipe)
        pipe = null
      }
    }

    def createSubprocess(processId: Array[Int], cmd: String): FileDescriptor = {
      val args = parse(cmd)
      val arg0 = args(0)
      Exec
        .createSubprocess(if (result != null) 1 else 0, arg0, args, null, scripts + "\nexit\n",
        processId)
    }

    def parse(cmd: String): Array[String] = {
      val PLAIN = 0
      val INQUOTE = 2
      val SKIP = 3

      var state = PLAIN
      val result: ArrayBuffer[String] = new ArrayBuffer[String]
      val builder = new StringBuilder()

      cmd foreach {
        ch => {
          state match {
            case PLAIN => {
              ch match {
                case c if Character.isWhitespace(c) => {
                  result += builder.toString
                  builder.clear()
                }
                case '"' => state = INQUOTE
                case _ => builder += ch
              }
            }
            case INQUOTE => {
              ch match {
                case '\\' => state = SKIP
                case '"' => state = PLAIN
                case _ => builder += ch
              }
            }
            case SKIP => {
              builder += ch
              state = INQUOTE
            }
          }
        }
      }

      if (builder.length > 0) {
        result += builder.toString
      }
      result.toArray
    }

    override def run() {
      pid(0) = -1
      try {
        if (this.asroot) {
          pipe = createSubprocess(pid, root_shell)
        } else {
          pipe = createSubprocess(pid, getShell)
        }
        if (pid(0) != -1) {
          exitcode = Exec.waitFor(pid(0))
        }
        if (result == null || pipe == null) return
        val stdout: InputStream = new FileInputStream(pipe)
        val buf = new Array[Byte](8192)
        var read: Int = 0
        while (stdout.available > 0) {
          read = stdout.read(buf)
          result.append(new String(buf, 0, read))
        }
      } catch {
        case ex: Exception => {
          Log.e(TAG, "Cannot execute command", ex)
          if (result != null) result.append("\n").append(ex)
        }
      } finally {
        if (pipe != null) {
          Exec.close(pipe)
        }
        if (pid(0) != -1) {
          Exec.hangupProcessGroup(pid(0))
        }
      }
    }
  }

}
