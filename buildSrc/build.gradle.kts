plugins {
    `kotlin-dsl`
}

apply(from = "../repositories.gradle.kts")

dependencies {
    implementation(libs.android.gradle)
    implementation(libs.kotlin.gradle)
}
