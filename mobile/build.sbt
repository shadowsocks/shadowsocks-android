enablePlugins(AndroidApp)
android.useSupportVectors

name := "shadowsocks"
version := "4.0.0"
versionCode := Some(174)

proguardOptions ++=
  "-keep class com.github.shadowsocks.System { *; }" ::
  "-dontwarn com.google.android.gms.internal.**" ::
  "-dontwarn com.j256.ormlite.**" ::
  "-dontwarn okio.**" ::
  "-dontwarn org.xbill.**" ::
  Nil

resConfigs := Seq("ja", "ru", "zh-rCN", "zh-rTW")

val supportLibsVersion = "25.1.0"
val playServicesVersion = "10.0.1"
resolvers += Resolver.jcenterRepo
libraryDependencies ++=
  "com.android.support" % "cardview-v7" % supportLibsVersion ::
  "com.android.support" % "customtabs" % supportLibsVersion ::
  "com.android.support" % "design" % supportLibsVersion ::
  "com.android.support" % "gridlayout-v7" % supportLibsVersion ::
  "com.android.support" % "preference-v14" % supportLibsVersion ::
  "com.futuremind.recyclerfastscroll" % "fastscroll" % "0.2.5" ::
  "com.evernote" % "android-job" % "1.1.4" ::
  "com.github.jorgecastilloprz" % "fabprogresscircle" % "1.01" ::
  "com.google.android.gms" % "play-services-ads" % playServicesVersion ::
  "com.google.android.gms" % "play-services-analytics" % playServicesVersion ::
  "com.google.android.gms" % "play-services-gcm" % playServicesVersion ::
  "com.j256.ormlite" % "ormlite-android" % "5.0" ::
  "com.mikepenz" % "crossfader" % "1.5.0" ::
  "com.mikepenz" % "fastadapter" % "2.1.5" ::
  "com.mikepenz" % "iconics-core" % "2.8.2" ::
  "com.mikepenz" % "materialdrawer" % "5.8.1" ::
  "com.mikepenz" % "materialize" % "1.0.0" ::
  "com.squareup.okhttp3" % "okhttp" % "3.5.0" ::
  "com.twofortyfouram" % "android-plugin-api-for-locale" % "1.0.2" ::
  "dnsjava" % "dnsjava" % "2.1.7" ::
  "eu.chainfire" % "libsuperuser" % "1.0.0.201608240809" ::
  "net.glxn.qrgen" % "android" % "2.0" ::
  Nil
