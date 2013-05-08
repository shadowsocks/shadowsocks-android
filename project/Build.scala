import sbt._
import sbt.Keys._

import org.scalasbt.androidplugin._
import org.scalasbt.androidplugin.AndroidKeys._

object App {
  val version = "1.4.2"
  val versionCode = 24
}

object General {

  val settings = Defaults.defaultSettings ++ Seq (
    name := "shadowsocks",
    version := App.version,
    versionCode := App.versionCode,
    scalaVersion := "2.9.2",
    compileOrder := CompileOrder.JavaThenScala,
    platformName in Android := "android-16",
    resolvers += "madeye-maven" at "http://madeye-maven-repository.googlecode.com/git",
    resolvers += "central-maven" at "http://repo.maven.apache.org/maven2"
  )
  
  val pgOptions = Seq("-keep class android.support.v4.app.** { *; }",
          "-keep interface android.support.v4.app.** { *; }",
          "-keep class com.actionbarsherlock.** { *; }",
          "-keep interface com.actionbarsherlock.** { *; }",
          "-keep class org.jraf.android.backport.** { *; }",
          "-keep class com.github.shadowsocks.** { *; }",
          "-keepattributes *Annotation*").mkString(" ")

  val proguardSettings = Seq (
    useProguard in Android := true,
    proguardOption in Android := pgOptions
  )

  lazy val fullAndroidSettings =
    General.settings ++
    AndroidProject.androidSettings ++
    TypedResources.settings ++
    proguardSettings ++
    AndroidManifestGenerator.settings ++
    AndroidMarketPublish.settings ++ Seq (
      cachePasswords in Android := true, 
      libraryDependencies += "org.scalatest" %% "scalatest" % "1.8" % "test"
    )
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
