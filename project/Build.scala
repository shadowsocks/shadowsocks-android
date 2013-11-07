import sbt._
import sbt.Keys._

import sbtandroid._
import sbtandroid.AndroidPlugin._

object App {
  val version = "2.0.3"
  val versionCode = 54
}

object General {

  val settings = Defaults.defaultSettings ++ Seq (
    name := "shadowsocks",
    version := App.version,
    versionCode := App.versionCode,
    scalaVersion := "2.10.3",
    compileOrder := CompileOrder.JavaThenScala,
    platformName := "android-16",
    resolvers += "madeye-maven" at "http://madeye-maven-repository.googlecode.com/git",
    resolvers += "central-maven" at "http://repo.maven.apache.org/maven2"
  )
  
  val pgOptions = Seq("-keep class android.support.v4.app.** { *; }",
          "-keep interface android.support.v4.app.** { *; }",
          "-keep class com.actionbarsherlock.** { *; }",
          "-keep interface com.actionbarsherlock.** { *; }",
          "-keep class org.jraf.android.backport.** { *; }",
          "-keep class com.github.shadowsocks.** { *; }",
          "-keep class * extends com.j256.ormlite.** { *; }",
          "-keepattributes *Annotation*")

  val proguardSettings = Seq (
    useProguard := true,
    proguardOptions := pgOptions
  )

  val miscSettings = Seq (
    cachePasswords := true
  )

  lazy val fullAndroidSettings =
    General.settings ++
    AndroidPlugin.androidDefaults ++
    proguardSettings ++
    miscSettings
}

object AndroidBuild extends Build {

  val deploy = TaskKey[Unit]("deploy", "deploy to the build server")

  val deployTask = deploy := {
    "./deploy target shadowsocks-compile-" + App.version + ".apk" !;
    println("Deploy successfully")
  }

  lazy val main = Project (
    "shadowsocks",
    file("."),
    settings = General.fullAndroidSettings ++ Seq(deployTask)
  )
}
