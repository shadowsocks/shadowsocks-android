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

package com.github.shadowsocks

import java.io.{File, FileOutputStream, IOException}
import java.util
import java.util.Locale
import java.util.concurrent.TimeUnit

import android.annotation.SuppressLint
import android.app.Application
import android.content._
import android.content.res.Configuration
import android.os.{Build, LocaleList}
import android.preference.PreferenceManager
import android.support.v7.app.AppCompatDelegate
import android.util.Log
import com.evernote.android.job.JobManager
import com.github.shadowsocks.acl.DonaldTrump
import com.github.shadowsocks.database.{DBHelper, Profile, ProfileManager}
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils._
import com.google.android.gms.analytics.{GoogleAnalytics, HitBuilders, StandardExceptionParser, Tracker}
import com.google.android.gms.common.api.ResultCallback
import com.google.android.gms.tagmanager.{ContainerHolder, TagManager}
import com.j256.ormlite.logger.LocalLog
import eu.chainfire.libsuperuser.Shell

import scala.collection.mutable.ArrayBuffer

object ShadowsocksApplication {
  var app: ShadowsocksApplication = _

  private final val TAG = "ShadowsocksApplication"

  // The ones in Locale doesn't have script included
  private final lazy val SIMPLIFIED_CHINESE =
    if (Build.VERSION.SDK_INT >= 21) Locale.forLanguageTag("zh-Hans-CN") else Locale.SIMPLIFIED_CHINESE
  private final lazy val TRADITIONAL_CHINESE =
    if (Build.VERSION.SDK_INT >= 21) Locale.forLanguageTag("zh-Hant-TW") else Locale.TRADITIONAL_CHINESE
}

class ShadowsocksApplication extends Application {
  import ShadowsocksApplication._

  final val SIG_FUNC = "getSignature"
  var containerHolder: ContainerHolder = _
  lazy val tracker: Tracker = GoogleAnalytics.getInstance(this).newTracker(R.xml.tracker)
  lazy val settings: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(this)
  lazy val editor: SharedPreferences.Editor = settings.edit
  lazy val profileManager = new ProfileManager(new DBHelper(this))

  def isNatEnabled: Boolean = settings.getBoolean(Key.isNAT, false)
  def isVpnEnabled: Boolean = !isNatEnabled

  // send event
  def track(category: String, action: String): Unit = tracker.send(new HitBuilders.EventBuilder()
    .setAction(action)
    .setLabel(BuildConfig.VERSION_NAME)
    .build())
  def track(t: Throwable): Unit = tracker.send(new HitBuilders.ExceptionBuilder()
    .setDescription(new StandardExceptionParser(this, null).getDescription(Thread.currentThread.getName, t))
    .setFatal(false)
    .build())

  def profileId: Int = settings.getInt(Key.id, 0)
  def profileId(i: Int): Unit = editor.putInt(Key.id, i).apply()
  def currentProfile: Option[Profile] = profileManager.getProfile(profileId)

  def switchProfile(id: Int): Profile = {
    val result = profileManager.getProfile(id) getOrElse profileManager.createProfile()
    profileId(result.id)
    result
  }

  private def checkChineseLocale(locale: Locale): Locale = if (locale.getLanguage == "zh") locale.getCountry match {
    case "CN" | "TW" => null            // already supported
    case _ => locale.getScript match {  // fallback to the corresponding script
      case "Hans" => SIMPLIFIED_CHINESE
      case "Hant" => TRADITIONAL_CHINESE
      case script =>
        Log.w(TAG, "Unknown zh locale script: %s. Falling back to trying countries...".format(script))
        locale.getCountry match {
          case "SG" => SIMPLIFIED_CHINESE
          case "HK" | "MO" => TRADITIONAL_CHINESE
          case _ =>
            Log.w(TAG, "Unknown zh locale: %s. Falling back to zh-Hans-CN...".format(locale.toLanguageTag))
            SIMPLIFIED_CHINESE
        }
    }
  } else null

  @SuppressLint(Array("NewApi"))
  private def checkChineseLocale(config: Configuration): Unit = if (Build.VERSION.SDK_INT >= 24) {
    val localeList = config.getLocales
    val newList = new Array[Locale](localeList.size())
    var changed = false
    for (i <- 0 until localeList.size()) {
      val locale = localeList.get(i)
      val newLocale = checkChineseLocale(locale)
      if (newLocale == null) newList(i) = locale else {
        newList(i) = newLocale
        changed = true
      }
    }
    if (changed) {
      val newConfig = new Configuration(config)
      newConfig.setLocales(new LocaleList(newList.distinct: _*))
      val res = getResources
      res.updateConfiguration(newConfig, res.getDisplayMetrics)
    }
  } else {
    //noinspection ScalaDeprecation
    val newLocale = checkChineseLocale(config.locale)
    if (newLocale != null) {
      val newConfig = new Configuration(config)
      //noinspection ScalaDeprecation
      newConfig.locale = newLocale
      val res = getResources
      res.updateConfiguration(newConfig, res.getDisplayMetrics)
    }
  }

  override def onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    checkChineseLocale(newConfig)
  }

  override def onCreate() {
    app = this
    if (!BuildConfig.DEBUG) java.lang.System.setProperty(LocalLog.LOCAL_LOG_LEVEL_PROPERTY, "ERROR")
    AppCompatDelegate.setCompatVectorFromResourcesEnabled(true)
    checkChineseLocale(getResources.getConfiguration)
    val tm = TagManager.getInstance(this)
    val pending = tm.loadContainerPreferNonDefault("GTM-NT8WS8", R.raw.gtm_default_container)
    val callback = new ResultCallback[ContainerHolder] {
      override def onResult(holder: ContainerHolder) {
        if (!holder.getStatus.isSuccess) {
          return
        }
        containerHolder = holder
        val container = holder.getContainer
        container.registerFunctionCallMacroCallback(SIG_FUNC,
          (functionName: String, parameters: util.Map[String, AnyRef]) => {
            if (functionName == SIG_FUNC) {
              Utils.getSignature(getApplicationContext)
            }
            null
          })
      }
    }
    pending.setResultCallback(callback, 2, TimeUnit.SECONDS)
    JobManager.create(this).addJobCreator(DonaldTrump)

    TcpFastOpen.enabled(settings.getBoolean(Key.tfo, TcpFastOpen.sendEnabled))
  }

  def refreshContainerHolder() {
    val holder = app.containerHolder
    if (holder != null) holder.refresh()
  }

  def crashRecovery() {
    val cmd = new ArrayBuffer[String]()

    for (task <- Executable.EXECUTABLES) {
      cmd.append("killall lib%s.so".formatLocal(Locale.ENGLISH, task))
      cmd.append("rm -f %1$s/%2$s-nat.conf %1$s/%2$s-vpn.conf"
        .formatLocal(Locale.ENGLISH, getFilesDir.getAbsolutePath, task))
    }
    if (app.isNatEnabled) {
      cmd.append("iptables -t nat -F OUTPUT")
      cmd.append("echo done")
      val result = Shell.SU.run(cmd.toArray)
      if (result != null && !result.isEmpty) return // fallback to SH
    }
    Shell.SH.run(cmd.toArray)
  }

  def copyAssets() {
    val assetManager = getAssets
    for (dir <- List("acl", "overture")) {
      var files: Array[String] = null
      try files = assetManager.list(dir) catch {
        case e: IOException =>
          Log.e(TAG, e.getMessage)
          app.track(e)
      }
      if (files != null) for (file <- files) autoClose(assetManager.open(dir + "/" + file))(in =>
        autoClose(new FileOutputStream(new File(getFilesDir, file)))(out =>
          IOUtils.copy(in, out)))
    }
    editor.putInt(Key.currentVersionCode, BuildConfig.VERSION_CODE).apply()
  }

  def updateAssets(): Unit = if (settings.getInt(Key.currentVersionCode, -1) != BuildConfig.VERSION_CODE) copyAssets()

  def listenForPackageChanges(callback: => Unit): BroadcastReceiver = {
    val filter = new IntentFilter(Intent.ACTION_PACKAGE_ADDED)
    filter.addAction(Intent.ACTION_PACKAGE_REMOVED)
    filter.addDataScheme("package")
    val result: BroadcastReceiver = (_: Context, intent: Intent) =>
      if (intent.getAction != Intent.ACTION_PACKAGE_REMOVED || !intent.getBooleanExtra(Intent.EXTRA_REPLACING, false))
        callback
    app.registerReceiver(result, filter)
    result
  }
}
