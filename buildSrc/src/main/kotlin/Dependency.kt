@file:Suppress("SpellCheckingInspection")

private const val coreVersion = "1.5.0-alpha04"
private const val appCompatVersion = "1.2.0"
private const val constraintLayoutVersion = "2.0.4"
private const val fragmentVersion = "1.3.0-beta01"
private const val materialVersion = "1.2.1"
private const val lifecycleVersion = "2.3.0-beta01"
private const val workVersion = "2.4.0"
private const val preferenceVersion = "1.1.1"
private const val gsonVersion = "2.8.6"
private const val coroutinesVersion = "1.4.0"
private const val desugarVersion = "1.1.0"
private const val cameraxVersion = "1.0.0-beta11"
private const val dnsJavaVersion = "3.3.1"
private const val timberVersion = "4.7.1"
private const val zxingVersion = "3.4.1"
private const val browserVersion = "1.2.0"
private const val drawerLayoutVersion = "1.1.1"
const val roomVersion = "2.2.5"

object Plugins {
    const val kotlinAndroid = "kotlin-android"
    const val kotlinKapt = "kotlin-kapt"
    const val kotlinExtensions = "kotlin-android-extensions"
    const val androidLibrary = "com.android.library"
    const val androidApplication = "com.android.application"
}

object Libs {
    const val core = "androidx.core:core-ktx:$coreVersion"
    const val desugar = "com.android.tools:desugar_jdk_libs:$desugarVersion"
    const val appCompat = "androidx.appcompat:appcompat:$appCompatVersion"
    const val fragment = "androidx.fragment:fragment-ktx:$fragmentVersion"
    const val preference = "androidx.preference:preference:$preferenceVersion"
    const val constraintLayout =
        "androidx.constraintlayout:constraintlayout:$constraintLayoutVersion"
    const val material = "com.google.android.material:material:$materialVersion"
    const val room = "androidx.room:room-runtime:$roomVersion"
    const val gson = "com.google.code.gson:gson:$gsonVersion"
    const val dnsJava = "dnsjava:dnsjava:$dnsJavaVersion"
    const val timber = "com.jakewharton.timber:timber:$timberVersion"
    const val zxing = "com.google.zxing:core:$zxingVersion"
    const val browser = "androidx.browser:browser:$browserVersion"
    const val drawerLayout = "androidx.drawerlayout:drawerlayout:$drawerLayoutVersion"

    const val roomKapt = "androidx.room:room-compiler:$roomVersion"

    val coroutines = arrayOf(
        "org.jetbrains.kotlinx:kotlinx-coroutines-core:$coroutinesVersion",
        "org.jetbrains.kotlinx:kotlinx-coroutines-android:$coroutinesVersion",
        "org.jetbrains.kotlinx:kotlinx-coroutines-play-services:$coroutinesVersion"
    )
    val lifecycle = arrayOf(
        "androidx.lifecycle:lifecycle-viewmodel-ktx:$lifecycleVersion",
        "androidx.lifecycle:lifecycle-livedata-ktx:$lifecycleVersion",
        "androidx.lifecycle:lifecycle-runtime-ktx:$lifecycleVersion",
        "androidx.lifecycle:lifecycle-viewmodel-savedstate:$lifecycleVersion",
        "androidx.lifecycle:lifecycle-common-java8:$lifecycleVersion"
    )
    val workManager = arrayOf(
        "androidx.work:work-runtime-ktx:$workVersion",
        "androidx.work:work-gcm:$workVersion"
    )
    val camera = arrayOf(
        "androidx.camera:camera-camera2:$cameraxVersion",
        "androidx.camera:camera-lifecycle:$cameraxVersion",
        "androidx.camera:camera-view:1.0.0-alpha18"
    )
}