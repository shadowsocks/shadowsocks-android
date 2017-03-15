lazy val commonSettings = Seq(
  scalaVersion := "2.11.8",
  dexMaxHeap := "4g",

  organization := "com.github.shadowsocks",

  platformTarget := "android-25",

  compileOrder := CompileOrder.JavaThenScala,
  javacOptions ++= "-source" :: "1.7" :: "-target" :: "1.7" :: Nil,
  scalacOptions ++= "-target:jvm-1.7" :: "-Xexperimental" :: Nil,
  ndkArgs := "-j" :: java.lang.Runtime.getRuntime.availableProcessors.toString :: Nil,

  proguardVersion := "5.3.2",
  proguardCache := Seq(),

  shrinkResources := true,
  typedResources := false,

  resConfigs := Seq("ja", "ko", "ru", "zh-rCN", "zh-rTW")
)

val supportLibsVersion = "25.2.0"
lazy val root = Project(id = "shadowsocks-android", base = file("."))
  .settings(commonSettings)
  .aggregate(plugin, mobile)

install in Android := (install in (mobile, Android)).value
run in Android := (run in (mobile, Android)).evaluated

lazy val plugin = project
  .settings(commonSettings)
  .settings(
    libraryDependencies += "com.android.support" % "preference-v14" % supportLibsVersion
  )

lazy val mobile = project
  .settings(commonSettings)
  .settings(
    libraryDependencies ++=
      "com.android.support" % "cardview-v7" % supportLibsVersion ::
      "com.android.support" % "customtabs" % supportLibsVersion ::
      "com.android.support" % "design" % supportLibsVersion ::
      "com.android.support" % "gridlayout-v7" % supportLibsVersion ::
      Nil
  )
  .dependsOn(plugin)
