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
    api("androidx.core:core-ktx:1.13.1")
    api("androidx.fragment:fragment-ktx:1.8.2")
    api("com.google.android.material:material:1.12.0")
}
