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
import java.util.Locale

import android.annotation.SuppressLint
import android.app.{Application, NotificationChannel, NotificationManager}
import android.content._
import android.content.pm.{PackageInfo, PackageManager}
import android.content.res.Configuration
import android.os.{Build, LocaleList}
import android.support.v7.app.AppCompatDelegate
import android.util.Log
import com.evernote.android.job.JobManager
import com.github.shadowsocks.acl.DonaldTrump
import com.github.shadowsocks.bg.{ProxyService, TransproxyService, VpnService}
import com.github.shadowsocks.database.{DBHelper, Profile, ProfileManager}
import com.github.shadowsocks.preference.{BottomSheetPreferenceDialogFragment, IconListPreference, OrmLitePreferenceDataStore}
import com.github.shadowsocks.utils.CloseUtils._
import com.github.shadowsocks.utils._
import com.google.android.gms.analytics.{GoogleAnalytics, HitBuilders, StandardExceptionParser, Tracker}
import com.google.firebase.FirebaseApp
import com.google.firebase.remoteconfig.FirebaseRemoteConfig
import com.j256.ormlite.logger.LocalLog
import com.takisoft.fix.support.v7.preference.PreferenceFragmentCompat

import scala.collection.JavaConversions._

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

  lazy val remoteConfig: FirebaseRemoteConfig = FirebaseRemoteConfig.getInstance()
  lazy val tracker: Tracker = GoogleAnalytics.getInstance(this).newTracker(R.xml.tracker)
  private lazy val dbHelper = new DBHelper(this)
  lazy val profileManager = new ProfileManager(dbHelper)
  lazy val dataStore = new OrmLitePreferenceDataStore(dbHelper)

  def usingVpnMode: Boolean = dataStore.serviceMode == Key.modeVpn
  def serviceClass: Class[_] = app.dataStore.serviceMode match {
    case Key.modeProxy => classOf[ProxyService]
    case Key.modeVpn => classOf[VpnService]
    case Key.modeTransproxy => classOf[TransproxyService]
  }

  // send event
  def track(category: String, action: String): Unit = tracker.send(new HitBuilders.EventBuilder()
    .setAction(action)
    .setLabel(BuildConfig.VERSION_NAME)
    .build())
  def track(t: Throwable): Unit = tracker.send(new HitBuilders.ExceptionBuilder()
    .setDescription(new StandardExceptionParser(this, null).getDescription(Thread.currentThread.getName, t))
    .setFatal(false)
    .build())

  def currentProfile: Option[Profile] = profileManager.getProfile(dataStore.profileId)

  def switchProfile(id: Int): Profile = {
    val result = profileManager.getProfile(id) getOrElse profileManager.createProfile()
    dataStore.profileId = result.id
    result
  }

  private def checkChineseLocale(locale: Locale): Locale = if (locale.getLanguage == "zh") locale.getCountry match {
    case "CN" | "TW" => null
    case _ => (if (Build.VERSION.SDK_INT >= 21) locale.getScript else null) match {
      case "Hans" => SIMPLIFIED_CHINESE
      case "Hant" => TRADITIONAL_CHINESE
      case script =>
        Log.w(TAG, "Unknown zh locale script: %s. Falling back to trying countries...".format(script))
        locale.getCountry match {
          case "SG" => SIMPLIFIED_CHINESE
          case "HK" | "MO" => TRADITIONAL_CHINESE
          case _ =>
            Log.w(TAG, "Unknown zh locale: %s. Falling back to zh-Hans-CN..."
              .formatLocal(Locale.ENGLISH, if (Build.VERSION.SDK_INT >= 21) locale.toLanguageTag else locale))
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

    FirebaseApp.initializeApp(this)
    remoteConfig.setDefaults(R.xml.default_configs)
    remoteConfig.fetch().addOnCompleteListener(task =>
        if (task.isSuccessful) remoteConfig.activateFetched() else Log.e(TAG, "Failed to fetch config"))

    JobManager.create(this).addJobCreator(DonaldTrump)
    PreferenceFragmentCompat.registerPreferenceFragment(classOf[IconListPreference],
      classOf[BottomSheetPreferenceDialogFragment])

    TcpFastOpen.enabled(dataStore.getBoolean(Key.tfo, TcpFastOpen.sendEnabled))

    if (dataStore.getLong(Key.assetUpdateTime, -1) != info.lastUpdateTime) copyAssets()

    if (Build.VERSION.SDK_INT >= 26) {
      val nm = getSystemService(classOf[NotificationManager])
      nm.createNotificationChannels(List(
        new NotificationChannel("service-vpn", getText(R.string.service_vpn), NotificationManager.IMPORTANCE_LOW),
        new NotificationChannel("service-proxy", getText(R.string.service_proxy), NotificationManager.IMPORTANCE_LOW),
        new NotificationChannel("service-transproxy", getText(R.string.service_transproxy),
          NotificationManager.IMPORTANCE_LOW)
      ))
      nm.deleteNotificationChannel("service-nat") // NAT mode is gone for good
    }
  }

  lazy val info: PackageInfo = getPackageManager.getPackageInfo(getPackageName, PackageManager.GET_SIGNATURES)

  def copyAssets() {
    val assetManager = getAssets
    for (dir <- Array("acl", "overture")) {
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
    dataStore.putLong(Key.assetUpdateTime, info.lastUpdateTime)
  }

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
