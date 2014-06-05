import android.Keys._

import android.Dependencies.{apklib,aar}

android.Plugin.androidBuild

platformTarget in Android := "android-16"

name := "shadowsocks"

libraryDependencies ++= Seq(
  "com.google.android" % "support-v4" % "18.0.0",
  "com.google.android" % "analytics" % "3.01",
  "dnsjava" % "dnsjava" % "2.1.5",
  "org.scalaj" %% "scalaj-http" % "0.3.10",
  "commons-net" % "commons-net" % "3.3",
  "com.google.zxing" % "android-integration" % "2.2"
)

libraryDependencies ++= Seq(
  apklib("com.actionbarsherlock" % "actionbarsherlock" % "4.4.0"), 
  apklib("net.saik0.android.unifiedpreference" % "unifiedpreference" % "0.0.2"),
  apklib("org.jraf" % "android-switch-backport" % "1.0"),
  apklib("net.simonvt.menudrawer" % "menudrawer" % "3.0.4"),
  "com.google.android.gms" % "play-services" % "4.4.52"
)

libraryDependencies ++= Seq(
  "de.keyboardsurfer.android.widget" % "crouton" % "1.8.1",
  "com.nostra13.universalimageloader" % "universal-image-loader" % "1.8.4",
  "com.j256.ormlite" % "ormlite-core" % "4.47",
  "com.j256.ormlite" % "ormlite-android" % "4.47"
)

