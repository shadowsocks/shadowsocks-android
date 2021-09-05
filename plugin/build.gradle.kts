plugins {
    id("com.android.library")
    id("com.vanniktech.maven.publish")
    kotlin("android")
    id("kotlin-parcelize")
}

setupCommon()

android {
    defaultConfig {
        versionCode = findProperty("VERSION_CODE").toString().toInt()
        versionName = findProperty("VERSION_NAME").toString()
    }

    lintOptions.informational("GradleDependency")
}

dependencies {
    api(kotlin("stdlib-jdk8"))
    api("androidx.core:core-ktx:1.0.0")
    api("androidx.fragment:fragment-ktx:1.3.0")
    api("com.google.android.material:material:1.1.0")
}
