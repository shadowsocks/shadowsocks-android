plugins {
    `kotlin-dsl`
}

apply(from = "../repositories.gradle.kts")

dependencies {
    implementation(rootProject.extra["androidPlugin"].toString())
    implementation(kotlin("gradle-plugin", rootProject.extra["kotlinVersion"].toString()))
}
