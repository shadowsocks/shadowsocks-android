import android.Keys._

android.Plugin.androidBuild

platformTarget in Android := "android-24"

name := "shadowsocksr"

applicationId  := "in.zhaoj.shadowsocksr"

scalaVersion := "2.11.8"

compileOrder in Compile := CompileOrder.JavaThenScala

javacOptions ++= Seq("-source", "1.6", "-target", "1.6")

scalacOptions ++= Seq("-target:jvm-1.6", "-Xexperimental")

ndkJavah in Android := List()

ndkBuild in Android := List()

shrinkResources in Android := true

typedResources in Android := false

resConfigs in Android := Seq("ru", "zh", "zh-rCN")

dexMaxHeap in Android := "4g"

resolvers += Resolver.jcenterRepo

resolvers += "JRAF" at "http://JRAF.org/static/maven/2"

useSupportVectors

libraryDependencies ++= Seq(
  "dnsjava" % "dnsjava" % "2.1.7",
  "com.github.kevinsawicki" % "http-request" % "6.0",
  "eu.chainfire" % "libsuperuser" % "1.0.0.201607041850",
  "net.glxn.qrgen" % "android" % "2.0",
  //"com.google.android.gms" % "play-services-ads" % "9.4.0",
  "com.google.android.gms" % "play-services-analytics" % "9.4.0",
  "com.android.support" % "design" % "24.1.1",
  "com.android.support" % "gridlayout-v7" % "24.1.1",
  "com.android.support" % "cardview-v7" % "24.1.1",
  "com.github.jorgecastilloprz" % "fabprogresscircle" % "1.01",
  "com.j256.ormlite" % "ormlite-android" % "5.0",
  "com.twofortyfouram" % "android-plugin-api-for-locale" % "1.0.2",
  "com.github.clans" % "fab" % "1.6.4",
  "me.dm7.barcodescanner" % "zxing" % "1.9"
)

proguardVersion in Android := "5.2.1"

proguardCache in Android := Seq()

proguardOptions in Android ++= Seq(
  "-keep class com.github.shadowsocks.System { *; }",
  "-dontwarn com.google.android.gms.internal.**",
  "-dontwarn com.j256.ormlite.**",
  "-dontwarn org.xbill.**")

lazy val nativeBuild = TaskKey[Unit]("native-build", "Build native executables")

nativeBuild := {
  val logger = streams.value.log
  Process("./build.sh") ! logger match {
    case 0 => // Success!
    case n => sys.error(s"Native build script exit code: $n")
  }
}
