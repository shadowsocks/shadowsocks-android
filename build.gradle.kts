// Top-level build file where you can add configuration options common to all sub-projects/modules.

plugins {
    id("com.github.ben-manes.versions") version "0.28.0"
}

buildscript {
    extra.apply {
        set("java_version", JavaVersion.VERSION_1_8)
        set("min_sdk_version", 21)
        set("sdk_version", 29)
        set("compile_sdk_version", 29)
        set("lifecycle_version", "2.2.0")
        set("desugar_libs_version", "1.0.5")
        set("junit_version", "4.13")
        set("android_test_version", "1.2.0")
        set("android_espresso_version", "3.2.0")
        set("version_code", 5000650)
        set("version_name", "5.0.6-nightly")
        set("res_configs", listOf("ar", "es", "fa", "fr", "ja", "ko", "ru", "tr", "zh-rCN", "zh-rTW"))
    }

    repositories {
        google()
        jcenter()
        maven("https://maven.fabric.io/public")
    }

    dependencies {
        classpath(kotlin("gradle-plugin", "1.3.71"))
        classpath("com.android.tools.build:gradle:4.0.0-beta04")
        classpath("com.google.android.gms:oss-licenses-plugin:0.10.2")
        classpath("com.google.gms:google-services:4.3.3")
        classpath("com.vanniktech:gradle-maven-publish-plugin:0.11.1")
        classpath("io.fabric.tools:gradle:1.31.2")
    }
}

allprojects {
    repositories {
        google()
        jcenter()
    }
}

tasks.register("clean", Delete::class) {
    delete(rootProject.buildDir)
}

tasks.withType<com.github.benmanes.gradle.versions.updates.DependencyUpdatesTask> {
    checkForGradleUpdate = true
    outputFormatter = "json"
    outputDir = "build/dependencyUpdates"
    reportfileName = "report"
}
