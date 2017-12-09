enablePlugins(AndroidApp)
enablePlugins(AndroidGms)

android.useSupportVectors

name := "shadowsocks"
version := "4.3.3"
versionCode := Some(199)

proguardOptions ++=
  "-dontwarn com.google.android.gms.internal.**" ::
  "-dontwarn com.j256.ormlite.**" ::
  "-dontwarn okhttp3.**" ::
  "-dontwarn okio.**" ::
  "-dontwarn org.xbill.**" ::
  "-keep class com.github.shadowsocks.JniHelper { *; }" ::
  "-dontwarn com.evernote.android.job.gcm.**" ::
  "-dontwarn com.evernote.android.job.util.GcmAvailableHelper" ::
  "-keep public class com.evernote.android.job.v21.PlatformJobService" ::
  "-keep public class com.evernote.android.job.v14.PlatformAlarmService" ::
  "-keep public class com.evernote.android.job.v14.PlatformAlarmReceiver" ::
  "-keep public class com.evernote.android.job.JobBootReceiver" ::
  "-keep public class com.evernote.android.job.JobRescheduleService" ::
  Nil

val playServicesVersion = "11.6.2"
resolvers += Resolver.jcenterRepo
libraryDependencies ++=
  "com.futuremind.recyclerfastscroll" % "fastscroll" % "0.2.5" ::
  "com.evernote" % "android-job" % "1.2.1" ::
  "com.github.jorgecastilloprz" % "fabprogresscircle" % "1.01" ::
  "com.google.android.gms" % "play-services-ads" % playServicesVersion ::
  "com.google.android.gms" % "play-services-analytics" % playServicesVersion ::
  "com.google.android.gms" % "play-services-gcm" % playServicesVersion ::
  "com.google.firebase" % "firebase-config" % playServicesVersion ::
  "com.j256.ormlite" % "ormlite-android" % "5.0" ::
  "com.mikepenz" % "crossfader" % "1.5.1" ::
  "com.mikepenz" % "fastadapter" % "3.0.4" ::
  "com.mikepenz" % "iconics-core" % "3.0.0" ::
  "com.mikepenz" % "materialdrawer" % "6.0.2" ::
  "com.mikepenz" % "materialize" % "1.1.2" ::
  "com.squareup.okhttp3" % "okhttp" % "3.9.1" ::
  "com.twofortyfouram" % "android-plugin-api-for-locale" % "1.0.2" ::
  "dnsjava" % "dnsjava" % "2.1.8" ::
  "eu.chainfire" % "libsuperuser" % "1.0.0.201704021214" ::
  "me.dm7.barcodescanner" % "zxing" % "1.9.8" ::
  "net.glxn.qrgen" % "android" % "2.0" ::
  Nil

packagingOptions := PackagingOptions(excludes =
  "META-INF/maven/com.squareup.okio/okio/pom.properties" ::
  "META-INF/maven/com.squareup.okio/okio/pom.xml" ::
  "META-INF/maven/com.squareup.okhttp3/okhttp/pom.properties" ::
  "META-INF/maven/com.squareup.okhttp3/okhttp/pom.xml" ::
  Nil)

lazy val goClean: TaskKey[Unit] = TaskKey[Unit]("go-clean", "Clean go build dependencies")
goClean := {
  IO.delete(baseDirectory(base => base / "src/overture/.deps").value)
  IO.delete(baseDirectory(base => base / "src/overture/bin").value)
  IO.delete(baseDirectory(base => base / "src/overture/go/bin").value)
  IO.delete(baseDirectory(base => base / "src/main/jni/overture").value)
}

lazy val goBuild: TaskKey[Unit] = TaskKey[Unit]("go-build", "Build go and overture")
goBuild := {
  Process(Seq("mobile/src/overture/make.bash", minSdkVersion.value)) ! streams.value.log match {
    case 0 => // Success!
    case n => sys.error(s"Native build script exit code: $n")
  }
}
