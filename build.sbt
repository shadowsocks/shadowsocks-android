libraryDependencies ++= Seq(
  "com.google.android" % "support-v4" % "r12",
  "com.google.android.analytics" % "analytics" % "2.0beta4",
  "dnsjava" % "dnsjava" % "2.1.5",
  "com.google.android.admob" % "admob" % "6.3.1"
)

libraryDependencies ++= Seq(
  "com.actionbarsherlock" % "actionbarsherlock" % "4.4.0" artifacts(Artifact("actionbarsherlock", "apklib", "apklib")),
  "net.saik0.android.unifiedpreference" % "unifiedpreference" % "0.0.2" artifacts(Artifact("unifiedpreference", "apklib", "apklib")),
  "org.jraf" % "android-switch-backport" % "1.0" artifacts(Artifact("android-switch-backport", "apklib", "apklib")),
  "net.simonvt.menudrawer" % "menudrawer" % "3.0.4" artifacts(Artifact("menudrawer", "apklib", "apklib"))
)

libraryDependencies ++= Seq(
  "de.keyboardsurfer.android.widget" % "crouton" % "1.7",
  "com.nostra13.universalimageloader" % "universal-image-loader" % "1.8.4",
  "com.j256.ormlite" % "ormlite-core" % "4.47",
  "com.j256.ormlite" % "ormlite-android" % "4.47"
)

