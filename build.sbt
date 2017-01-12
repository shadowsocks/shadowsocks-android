import android.Keys.proguardCache

lazy val commonSettings = Seq(
  scalaVersion := "2.11.8",
  dexMaxHeap := "4g",

  organization := "com.github.shadowsocks",

  platformTarget := "android-25",

  compileOrder := CompileOrder.JavaThenScala,
  javacOptions ++= "-source" :: "1.7" :: "-target" :: "1.7" :: Nil,
  scalacOptions ++= "-target:jvm-1.7" :: "-Xexperimental" :: Nil,

  proguardVersion := "5.3.2",
  proguardCache := Seq(),

  shrinkResources := true,
  typedResources := false
)

lazy val mobile = project.settings(commonSettings)
