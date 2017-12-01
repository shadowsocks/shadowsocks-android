lazy val commonSettings = Seq(
  scalaVersion := "2.11.12",
  dexMaxHeap := "4g",

  organization := "com.github.shadowsocks",

  platformTarget := "android-27",

  compileOrder := CompileOrder.JavaThenScala,
  javacOptions ++= "-source" :: "1.7" :: "-target" :: "1.7" :: Nil,
  scalacOptions ++= "-target:jvm-1.7" :: "-Xexperimental" :: Nil,
  ndkArgs := "-j" :: java.lang.Runtime.getRuntime.availableProcessors.toString :: Nil,
  ndkAbiFilter := Seq("armeabi-v7a", "arm64-v8a", "x86"),

  proguardVersion := "5.3.3",
  proguardCache := Seq(),

  shrinkResources := true,
  typedResources := false,

  resConfigs := Seq("fa", "ja", "ko", "ru", "zh-rCN", "zh-rTW"),

  resolvers += Resolver.jcenterRepo,
  resolvers += Resolver.bintrayRepo("gericop", "maven"),
  resolvers += "google" at "https://maven.google.com"
)

val supportLibsVersion = "27.0.2"
val takisoftFixVersion = "27.0.2.0"
lazy val root = Project(id = "shadowsocks-android", base = file("."))
  .settings(commonSettings)
  .aggregate(plugin, mobile)

install in Android := (install in (mobile, Android)).value
run in Android := (run in (mobile, Android)).evaluated

lazy val plugin = project
  .settings(commonSettings)
  .settings(
    libraryDependencies ++=
      "com.android.support" % "preference-v14" % supportLibsVersion ::
      "com.takisoft.fix" % "preference-v7" % takisoftFixVersion ::
      "com.takisoft.fix" % "preference-v7-simplemenu" % takisoftFixVersion ::
      Nil
  )

lazy val mobile = project
  .settings(commonSettings)
  .settings(
    libraryDependencies ++=
      "com.android.support" % "customtabs" % supportLibsVersion ::
      "com.android.support" % "design" % supportLibsVersion ::
      "com.android.support" % "gridlayout-v7" % supportLibsVersion ::
      Nil
  )
  .dependsOn(plugin)
