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
    lint {
        warning += "ExtraTranslation"
        warning += "ImpliedQuantity"
        warning += "UnusedAttribute"
        informational += "MissingTranslation"
        baseline = file("lint-baseline.xml")
        abortOnError = false
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
        "local-dns",
        "armv8",
        "neon",
        "aead-cipher-2022",
    ))
    exec = { spec, toolchain ->
        spec.environment("RUST_ANDROID_GRADLE_PYTHON_COMMAND", "python3")
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
    val coroutinesVersion = "1.6.2"
    val roomVersion = "2.4.2"
    val workVersion = "2.7.1"

    api(project(":plugin"))
    api("androidx.core:core-ktx:1.8.0")
    // https://android-developers.googleblog.com/2019/07/android-q-beta-5-update.html
    api("androidx.drawerlayout:drawerlayout:1.1.1")
    api("androidx.fragment:fragment-ktx:1.5.0")
    api("com.google.android.material:material:1.6.1")

    api("androidx.lifecycle:lifecycle-livedata-core-ktx:$lifecycleVersion")
    api("androidx.preference:preference:1.2.0")
    api("androidx.room:room-runtime:$roomVersion")
    api("androidx.work:work-multiprocess:$workVersion")
    api("androidx.work:work-runtime-ktx:$workVersion")
    api("com.google.android.gms:play-services-oss-licenses:17.0.0")
    api("com.google.code.gson:gson:2.9.0")
    api("com.google.firebase:firebase-analytics-ktx:21.1.0")
    api("com.google.firebase:firebase-crashlytics:18.2.11")
    api("com.jakewharton.timber:timber:5.0.1")
    api("dnsjava:dnsjava:3.5.1")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:$coroutinesVersion")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:$coroutinesVersion")
    kapt("androidx.room:room-compiler:$roomVersion")
    androidTestImplementation("androidx.room:room-testing:$roomVersion")
    androidTestImplementation("androidx.test.ext:junit-ktx:1.1.3")
}
