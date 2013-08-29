import sbt._
import sbt.Keys._

import sbtandroid._
import sbtandroid.AndroidPlugin._

object App {
  val version = "1.8.2"
  val versionCode = 47
}

object General {

  val settings = Defaults.defaultSettings ++ Seq (
    name := "shadowsocks",
    version := App.version,
    versionCode := App.versionCode,
    scalaVersion := "2.9.2",
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
          "-keepattributes *Annotation*")

  val proguardSettings = Seq (
    useProguard := true,
    proguardOptions := pgOptions
  )

  val miscSettings = Seq (
    cachePasswords := true,
    publicServer := "127.0.0.1"
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
    "./deploy target shadowsocks-" + App.version + ".apk" !;
    println("Deploy successfully")
  }

  lazy val main = Project (
    "shadowsocks",
    file("."),
    settings = General.fullAndroidSettings ++ Seq(deployTask)
  )
}
