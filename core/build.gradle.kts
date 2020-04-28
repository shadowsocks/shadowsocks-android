import com.android.build.gradle.internal.tasks.factory.dependsOn

plugins {
    id("com.android.library")
    id("org.mozilla.rust-android-gradle.rust-android")
    kotlin("android")
    kotlin("android.extensions")
    kotlin("kapt")
}

setupCore()

android {
    defaultConfig {
        consumerProguardFiles("proguard-rules.pro")

        externalNativeBuild.ndkBuild {
            abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            arguments("-j${Runtime.getRuntime().availableProcessors()}")
        }

        javaCompileOptions.annotationProcessorOptions.arguments = mapOf(
                "room.incremental" to "true",
                "room.schemaLocation" to "$projectDir/schemas"
        )
    }

    externalNativeBuild.ndkBuild.path("src/main/jni/Android.mk")

    sourceSets.getByName("androidTest") {
        assets.setSrcDirs(assets.srcDirs + files("$projectDir/schemas"))
    }
}

androidExtensions.isExperimental = true

cargo {
    module = "src/main/rust/shadowsocks-rust"
    libname = "sslocal"
    targets = listOf("arm", "arm64", "x86", "x86_64")
    profile = findProperty("CARGO_PROFILE")?.toString() ?: "release"
    extraCargoBuildArguments = listOf("--bin", libname!!)
    featureSpec.noDefaultBut(arrayOf(
            "sodium",
            "rc4",
            "aes-cfb",
            "aes-ctr",
            "camellia-cfb",
            "openssl-vendored",
            "local-flow-stat",
            "local-dns-relay"))
    exec = { spec, toolchain ->
        spec.environment("RUST_ANDROID_GRADLE_CC_LINK_ARG", "-o,target/${toolchain.target}/$profile/lib$libname.so")
    }
}

tasks.whenTaskAdded {
    when (name) {
        "javaPreCompileDebug", "javaPreCompileRelease" -> dependsOn("cargoBuild")
    }
}

tasks.register<Exec>("cargoClean") {
    executable("cargo")     // cargo.cargoCommand
    args("clean")
    workingDir("$projectDir/${cargo.module}")
}
tasks.clean.dependsOn("cargoClean")

dependencies {
    val coroutinesVersion = "1.3.5"
    val roomVersion = "2.2.5"
    val workVersion = "2.3.4"

    api(project(":plugin"))
    api("androidx.fragment:fragment-ktx:1.2.4")
    api("androidx.lifecycle:lifecycle-common-java8:$lifecycleVersion")
    api("androidx.lifecycle:lifecycle-livedata-core-ktx:$lifecycleVersion")
    api("androidx.preference:preference:1.1.1")
    api("androidx.room:room-runtime:$roomVersion")
    api("androidx.work:work-runtime-ktx:$workVersion")
    api("androidx.work:work-gcm:$workVersion")
    api("com.google.android.gms:play-services-oss-licenses:17.0.0")
    api("com.google.code.gson:gson:2.8.6")
    api("com.google.firebase:firebase-analytics-ktx:17.4.0")
    api("com.google.firebase:firebase-config-ktx:19.1.4")
    api("com.google.firebase:firebase-crashlytics:17.0.0")
    api("com.jakewharton.timber:timber:4.7.1")
    api("dnsjava:dnsjava:3.0.2")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:$coroutinesVersion")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:$coroutinesVersion")
    kapt("androidx.room:room-compiler:$roomVersion")
    androidTestImplementation("androidx.room:room-testing:$roomVersion")
    androidTestImplementation("androidx.test.ext:junit-ktx:1.1.1")
}
