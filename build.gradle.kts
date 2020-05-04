// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    id("com.github.ben-manes.versions") version "0.28.0"
}

buildscript {
    apply(from = "repositories.gradle.kts")

    repositories {
        google()
        jcenter()
        maven("https://plugins.gradle.org/m2/")
    }

    dependencies {
        classpath(rootProject.extra.get("androidPlugin").toString())
        classpath(kotlin("gradle-plugin", rootProject.extra.get("kotlinVersion").toString()))
        classpath("com.google.android.gms:oss-licenses-plugin:0.10.2")
        classpath("com.google.firebase:firebase-crashlytics-gradle:2.0.0")
        classpath("com.google.gms:google-services:4.3.3")
        classpath("com.vanniktech:gradle-maven-publish-plugin:0.11.1")
        classpath("gradle.plugin.org.mozilla.rust-android-gradle:plugin:0.8.3")
    }
}

allprojects {
    apply(from = "${rootProject.projectDir}/repositories.gradle.kts")
}

tasks.register<Delete>("clean") {
    delete(rootProject.buildDir)
}

// skip uploading the mapping to Crashlytics
subprojects {
    tasks.whenTaskAdded {
        if(name.contains("uploadCrashlyticsMappingFileRelease")) {
            enabled = false
        }
    }
}
