plugins {
    id("com.android.application")
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
    kotlin("android")
}

setupApp()

android {
    namespace = "com.github.shadowsocks.tv"
    defaultConfig {
        applicationId = "com.github.shadowsocks.tv"
        buildConfigField("boolean", "FULLSCREEN", "false")
    }
    flavorDimensions.add("market")
    productFlavors {
        create("freedom") {
            dimension = "market"
        }
        create("google") {
            dimension = "market"
            buildConfigField("boolean", "FULLSCREEN", "true")
        }
    }
}

dependencies {
    coreLibraryDesugaring(libs.desugar)
    implementation(libs.androidx.leanback.preference)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.test.runner)
}
