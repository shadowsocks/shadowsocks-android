import android.Keys._

android.Plugin.androidBuild

platformTarget in Android := "android-23"

name := "shadowsocks"

scalaVersion := "2.11.7"

compileOrder in Compile := CompileOrder.JavaThenScala

javacOptions ++= Seq("-source", "1.6", "-target", "1.6")

scalacOptions ++= Seq("-target:jvm-1.6", "-Xexperimental")

ndkJavah in Android := List()

ndkBuild in Android := List()

typedResources in Android := false

resolvers += Resolver.jcenterRepo

resolvers += "JRAF" at "http://JRAF.org/static/maven/2"

libraryDependencies ++= Seq(
  "dnsjava" % "dnsjava" % "2.1.7",
  "com.github.kevinsawicki" % "http-request" % "6.0",
  "commons-net" % "commons-net" % "3.3",
  "com.google.zxing" % "android-integration" % "3.2.1",
  "com.joanzapata.android" % "android-iconify" % "1.0.11",
  "net.glxn.qrgen" % "android" % "2.0",
  "net.simonvt.menudrawer" % "menudrawer" % "3.0.6",
  "com.google.android.gms" % "play-services-base" % "8.1.0",
  "com.google.android.gms" % "play-services-ads" % "8.1.0",
  "com.google.android.gms" % "play-services-analytics" % "8.1.0",
  "com.android.support" % "support-v4" % "23.1.0",
  "com.android.support" % "design" % "23.1.0",
  "com.nostra13.universalimageloader" % "universal-image-loader" % "1.9.4",
  "com.j256.ormlite" % "ormlite-core" % "4.48",
  "com.j256.ormlite" % "ormlite-android" % "4.48"
)

proguardOptions in Android ++= Seq("-keep class android.support.v4.app.** { *; }",
          "-keep interface android.support.v4.app.** { *; }",
          "-keep class android.support.v7.widget.Toolbar { <init>(...); }",
          "-keep class com.actionbarsherlock.** { *; }",
          "-keep interface com.actionbarsherlock.** { *; }",
          "-keep class org.jraf.android.backport.** { *; }",
          "-keep class com.github.shadowsocks.** { *; }",
          "-keep class * extends com.j256.ormlite.** { *; }",
          "-keep class com.joanzapata.** { *; }",
          "-keepattributes *Annotation*",
          "-dontwarn com.google.android.gms.internal.zzhu",
          "-dontwarn org.xbill.**",
          "-dontwarn com.actionbarsherlock.**")
