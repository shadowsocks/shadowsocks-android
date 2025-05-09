// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    alias(libs.plugins.versions)
    alias(libs.plugins.ksp) apply false
}

buildscript {
    apply(from = "repositories.gradle.kts")

    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }

    dependencies {
        classpath(libs.android.gradle)
        classpath(libs.dokka)
        classpath(libs.firebase.crashlytics.gradle)
        classpath(libs.google.oss.licenses)
        classpath(libs.google.services)
        classpath(libs.gradle.maven.publish)
        classpath(libs.kotlin.gradle)
        classpath(libs.rust.android)
    }
}

allprojects {
    apply(from = "${rootProject.projectDir}/repositories.gradle.kts")
}

// skip uploading the mapping to Crashlytics
subprojects {
    tasks.whenTaskAdded {
        if (name.contains("uploadCrashlyticsMappingFile")) enabled = false
    }
}
