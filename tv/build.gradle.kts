plugins {
    id(Plugins.androidApplication)
    id(Plugins.kotlinAndroid)
    id(Plugins.kotlinKapt)
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
}

setupApp().run {
    defaultConfig.applicationId = "com.github.shadowsocks.tv"
}

dependencies {
    implementation("androidx.leanback:leanback-preference:1.1.0-alpha05")
}
