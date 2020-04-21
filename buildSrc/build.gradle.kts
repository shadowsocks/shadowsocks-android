plugins {
    `kotlin-dsl`
}

apply(from = "../repositories.gradle.kts")

dependencies {
    implementation(rootProject.extra.get("androidPlugin").toString())
    implementation(kotlin("gradle-plugin", rootProject.extra.get("kotlinVersion").toString()))
}
