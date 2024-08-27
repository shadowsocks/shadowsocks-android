plugins {
    id("com.android.library")
    id("com.vanniktech.maven.publish")
    kotlin("android")
    id("kotlin-parcelize")
}

setupCommon()

android {
    namespace = "com.github.shadowsocks.plugin"
    lint.informational += "GradleDependency"
}

dependencies {
    api(kotlin("stdlib-jdk8"))
    api("androidx.core:core-ktx:1.7.0")
    api("androidx.fragment:fragment-ktx:1.5.5")
    api("com.google.android.material:material:1.6.0")
}
