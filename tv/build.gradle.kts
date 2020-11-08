plugins {
    id("com.android.application")
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
    kotlin("android")
    kotlin("kapt")
}

setupApp().run {
    defaultConfig.applicationId = "com.github.shadowsocks.tv"
}

dependencies {
    implementation("androidx.leanback:leanback-preference:1.1.0-alpha05")
}
