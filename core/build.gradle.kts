import com.android.build.gradle.internal.tasks.factory.dependsOn

plugins {
    id(Plugins.androidLibrary)
    id(Plugins.kotlinAndroid)
    id(Plugins.kotlinExtensions)
    id(Plugins.kotlinKapt)
    id("org.mozilla.rust-android-gradle.rust-android")
}

setupCore().run {
    defaultConfig {
        consumerProguardFiles("proguard-rules.pro")

        externalNativeBuild.ndkBuild {
            abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            arguments("-j${Runtime.getRuntime().availableProcessors()}")
        }

        javaCompileOptions.annotationProcessorOptions.arguments(
            mapOf(
                "room.incremental" to "true",
                "room.schemaLocation" to "$projectDir/schemas"
            )
        )
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
    featureSpec.noDefaultBut(
        arrayOf(
            "ring-aead-ciphers",
            "sodium",
            "rc4",
            "aes-cfb",
            "aes-ctr",
            "camellia-cfb",
            "openssl-vendored",
            "local-flow-stat",
            "local-dns-relay"
        )
    )
    exec = { spec, toolchain ->
        spec.environment(
            "RUST_ANDROID_GRADLE_LINKER_WRAPPER_PY",
            "$projectDir/$module/../linker-wrapper.py"
        )
        spec.environment(
            "RUST_ANDROID_GRADLE_TARGET",
            "target/${toolchain.target}/$profile/lib$libname.so"
        )
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
    api(project(":plugin"))
    api(*Libs.coroutines)
    api(Libs.appCompat)
    api(Libs.fragment)
    api(*Libs.lifecycle)
    api(Libs.preference)
    api(Libs.room)
    kapt(Libs.roomKapt)
    api(*Libs.workManager)
    api(Libs.gson)
    api(Libs.dnsJava)
    api(Libs.timber)
    api("com.google.android.gms:play-services-oss-licenses:17.0.0")
    api("com.google.firebase:firebase-analytics-ktx:18.0.0")
    api("com.google.firebase:firebase-config-ktx:20.0.0")
    api("com.google.firebase:firebase-crashlytics:17.2.2")
    androidTestImplementation("androidx.room:room-testing:$roomVersion")
    androidTestImplementation("androidx.test.ext:junit-ktx:1.1.2")
}
