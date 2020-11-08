plugins {
    id("com.github.ben-manes.versions") version "0.33.0"
}

buildscript {
    apply(from = "repositories.gradle.kts")

    repositories {
        google()
        jcenter()
        maven("https://plugins.gradle.org/m2/")
    }

    dependencies {
        val kotlinVersion = rootProject.extra.get("kotlinVersion").toString()
        classpath(rootProject.extra.get("androidPlugin").toString())
        classpath(kotlin("gradle-plugin", kotlinVersion))
        classpath("com.google.android.gms:oss-licenses-plugin:0.10.2")
        classpath("com.google.firebase:firebase-crashlytics-gradle:2.3.0")
        classpath("com.google.gms:google-services:4.3.4")
        classpath("com.vanniktech:gradle-maven-publish-plugin:0.13.0")
        classpath("gradle.plugin.org.mozilla.rust-android-gradle:plugin:0.8.3")
        classpath("org.jetbrains.dokka:dokka-gradle-plugin:$kotlinVersion")
    }
}

allprojects {
    apply(from = "${rootProject.projectDir}/repositories.gradle.kts")
    tasks.withType(JavaCompile::class) {
        options.encoding = java.nio.charset.StandardCharsets.UTF_8.toString()
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.buildDir)
}

// skip uploading the mapping to Crashlytics
subprojects {
    tasks.whenTaskAdded {
        if (name.contains("uploadCrashlyticsMappingFile")) enabled = false
    }
}
