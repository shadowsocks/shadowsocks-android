import com.android.build.gradle.internal.tasks.factory.dependsOn

plugins {
    id("com.android.library")
    id("org.mozilla.rust-android-gradle.rust-android")
    kotlin("android")
    kotlin("kapt")
    id("kotlin-parcelize")
}

setupCore()

android {
    defaultConfig {
        consumerProguardFiles("proguard-rules.pro")

        externalNativeBuild.ndkBuild {
            abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            arguments("-j${Runtime.getRuntime().availableProcessors()}")
        }

        kapt.arguments {
            arg("room.incremental", true)
            arg("room.schemaLocation", "$projectDir/schemas")
        }
    }

    externalNativeBuild.ndkBuild.path("src/main/jni/Android.mk")

    sourceSets.getByName("androidTest") {
        assets.setSrcDirs(assets.srcDirs + files("$projectDir/schemas"))
    }
}

cargo {
    module = "src/main/rust/shadowsocks-rust"
    libname = "sslocal"
    targets = listOf("arm", "arm64", "x86", "x86_64")
    profile = findProperty("CARGO_PROFILE")?.toString() ?: currentFlavor
    extraCargoBuildArguments = listOf("--bin", libname!!)
    featureSpec.noDefaultBut(arrayOf(
            "stream-cipher",
            "aead-cipher-extra",
            "logging",
            "local-flow-stat",
            "local-dns"))
    exec = { spec, toolchain ->
        spec.environment("RUST_ANDROID_GRADLE_LINKER_WRAPPER_PY", "$projectDir/$module/../linker-wrapper.py")
        spec.environment("RUST_ANDROID_GRADLE_TARGET", "target/${toolchain.target}/$profile/lib$libname.so")
    }
}

tasks.whenTaskAdded {
    when (name) {
        "mergeDebugJniLibFolders", "mergeReleaseJniLibFolders" -> dependsOn("cargoBuild")
    }
}

tasks.register<Exec>("cargoClean") {
    executable("cargo")     // cargo.cargoCommand
    args("clean")
    workingDir("$projectDir/${cargo.module}")
}
tasks.clean.dependsOn("cargoClean")

dependencies {
    val coroutinesVersion = "1.5.2"
    val roomVersion = "2.3.0"
    val workVersion = "2.7.0-beta01"

    api(project(":plugin"))
    api("androidx.core:core-ktx:1.6.0")
    // https://android-developers.googleblog.com/2019/07/android-q-beta-5-update.html
    api("androidx.drawerlayout:drawerlayout:1.1.1")
    api("androidx.fragment:fragment-ktx:1.3.6")
    api("com.google.android.material:material:1.4.0")

    api("androidx.lifecycle:lifecycle-livedata-core-ktx:$lifecycleVersion")
    api("androidx.preference:preference:1.1.1")
    api("androidx.room:room-runtime:$roomVersion")
    api("androidx.work:work-multiprocess:$workVersion")
    api("androidx.work:work-runtime-ktx:$workVersion")
    api("com.google.android.gms:play-services-oss-licenses:17.0.0")
    api("com.google.code.gson:gson:2.8.8")
    api("com.google.firebase:firebase-analytics-ktx:19.0.1")
    api("com.google.firebase:firebase-crashlytics:18.2.1")
    api("com.jakewharton.timber:timber:5.0.1")
    api("dnsjava:dnsjava:3.4.1")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:$coroutinesVersion")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:$coroutinesVersion")
    kapt("androidx.room:room-compiler:$roomVersion")
    androidTestImplementation("androidx.room:room-testing:$roomVersion")
    androidTestImplementation("androidx.test.ext:junit-ktx:1.1.3")
}
