// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    id("com.github.ben-manes.versions") version "0.51.0"
    id("com.google.devtools.ksp") version "2.0.20-1.0.24" apply false
}

buildscript {
    apply(from = "repositories.gradle.kts")

    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }

    dependencies {
        val kotlinVersion = rootProject.extra["kotlinVersion"].toString()
        classpath(rootProject.extra["androidPlugin"].toString())
        classpath(kotlin("gradle-plugin", kotlinVersion))
        classpath("com.google.android.gms:oss-licenses-plugin:0.10.6")
        classpath("com.google.firebase:firebase-crashlytics-gradle:3.0.2")
        classpath("com.google.gms:google-services:4.4.2")
        classpath("com.vanniktech:gradle-maven-publish-plugin:0.29.0")
        classpath("org.jetbrains.dokka:dokka-gradle-plugin:1.9.20")
        classpath("org.mozilla.rust-android-gradle:plugin:0.9.4")
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
