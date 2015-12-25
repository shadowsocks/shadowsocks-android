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

shrinkResources in Android := true

typedResources in Android := false

resolvers += Resolver.jcenterRepo

resolvers += "JRAF" at "http://JRAF.org/static/maven/2"

libraryDependencies ++= Seq(
  "dnsjava" % "dnsjava" % "2.1.7",
  "com.github.kevinsawicki" % "http-request" % "6.0",
  "commons-net" % "commons-net" % "3.4",
  "eu.chainfire" % "libsuperuser" % "1.0.0.201510071325",
  "com.google.zxing" % "android-integration" % "3.2.1",
  "net.glxn.qrgen" % "android" % "2.0",
  "com.google.android.gms" % "play-services-base" % "8.4.0",
  "com.google.android.gms" % "play-services-ads" % "8.4.0",
  "com.google.android.gms" % "play-services-analytics" % "8.4.0",
  "com.android.support" % "design" % "23.1.1",
  "com.github.jorgecastilloprz" % "fabprogresscircle" % "1.01",
  "com.j256.ormlite" % "ormlite-core" % "4.48",
  "com.j256.ormlite" % "ormlite-android" % "4.48",
  "com.twofortyfouram" % "android-plugin-api-for-locale" % "1.0.2"
)

proguardOptions in Android ++= Seq("-keep class com.github.shadowsocks.** { <init>(...); }",
          "-keep class com.github.shadowsocks.System { *; }",
          "-keepattributes *Annotation*",
          "-dontwarn org.xbill.**")
