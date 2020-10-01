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

dependencies {
    val cameraxVersion = "1.0.0-beta10"

    implementation("androidx.browser:browser:1.2.0")
    implementation("androidx.camera:camera-camera2:$cameraxVersion")
    implementation("androidx.camera:camera-lifecycle:$cameraxVersion")
    implementation("androidx.camera:camera-view:1.0.0-alpha17")
    implementation("androidx.constraintlayout:constraintlayout:2.0.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:$lifecycleVersion")
    implementation("com.google.firebase:firebase-ads:19.4.0")
    implementation("com.google.mlkit:barcode-scanning:16.0.3")
    implementation("com.google.zxing:core:3.4.1")
    implementation("com.takisoft.preferencex:preferencex-simplemenu:1.1.0")
    implementation("com.twofortyfouram:android-plugin-api-for-locale:1.0.4")
    implementation("me.zhanghai.android.fastscroll:library:1.1.4")
}
