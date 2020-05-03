plugins {
    id("com.android.application")
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
    kotlin("android")
    kotlin("android.extensions")
}

setupApp()

android.defaultConfig.applicationId = "com.github.shadowsocks"
androidExtensions.isExperimental = true

dependencies {
    implementation("androidx.browser:browser:1.2.0")
    implementation("androidx.constraintlayout:constraintlayout:2.0.0-beta4")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:$lifecycleVersion")
    implementation("com.google.android.gms:play-services-vision:20.0.0")
    implementation("com.google.firebase:firebase-ads:19.1.0")
    implementation("com.google.zxing:core:3.4.0")
    implementation("com.takisoft.preferencex:preferencex-simplemenu:1.1.0")
    implementation("com.twofortyfouram:android-plugin-api-for-locale:1.0.4")
    implementation("me.zhanghai.android.fastscroll:library:1.1.3")
    implementation("xyz.belvi.mobilevision:barcodescanner:2.0.3")
}
