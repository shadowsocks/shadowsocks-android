import com.android.build.gradle.internal.tasks.factory.dependsOn

plugins {
    id("com.android.library")
    id("com.google.devtools.ksp")
    id("org.mozilla.rust-android-gradle.rust-android")
    kotlin("android")
    id("kotlin-parcelize")
}

setupCore()

android {
    namespace = "com.github.shadowsocks.core"

    defaultConfig {
        consumerProguardFiles("proguard-rules.pro")

        externalNativeBuild.ndkBuild {
            abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
            arguments("-j${Runtime.getRuntime().availableProcessors()}")
        }

        ksp {
            arg("room.incremental", "true")
            arg("room.schemaLocation", "$projectDir/schemas")
        }
    }

    externalNativeBuild.ndkBuild.path("src/main/jni/Android.mk")

    sourceSets.getByName("androidTest") {
        assets.setSrcDirs(assets.srcDirs + files("$projectDir/schemas"))
    }

    buildFeatures.aidl = true
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
        "aead-cipher-2022",
    ))
    exec = { spec, toolchain ->
        run {
            try {
                Runtime.getRuntime().exec(arrayOf("python3", "-V"))
                spec.environment("RUST_ANDROID_GRADLE_PYTHON_COMMAND", "python3")
                project.logger.lifecycle("Python 3 detected.")
            } catch (e: java.io.IOException) {
                project.logger.lifecycle("No python 3 detected.")
                try {
                    Runtime.getRuntime().exec(arrayOf("python", "-V"))
                    spec.environment("RUST_ANDROID_GRADLE_PYTHON_COMMAND", "python")
                    project.logger.lifecycle("Python detected.")
                } catch (e: java.io.IOException) {
                    throw GradleException("No any python version detected. You should install the python first to compile project.")
                }
            }
            // https://developer.android.com/guide/practices/page-sizes#other-build-systems
            spec.environment("RUST_ANDROID_GRADLE_CC_LINK_ARG", "-Wl,-z,max-page-size=16384,-soname,lib$libname.so")
            spec.environment("RUST_ANDROID_GRADLE_LINKER_WRAPPER_PY", "$projectDir/$module/../linker-wrapper.py")
            spec.environment("RUST_ANDROID_GRADLE_TARGET", "target/${toolchain.target}/$profile/lib$libname.so")
        }
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
    api(libs.androidx.core.ktx)
    api(libs.androidx.lifecycle.livedata.core.ktx)
    api(libs.androidx.preference)
    api(libs.androidx.room.runtime)
    api(libs.androidx.work.multiprocess)
    api(libs.androidx.work.runtime.ktx)
    api(libs.dnsjava)
    api(libs.firebase.analytics)
    api(libs.firebase.crashlytics)
    api(libs.kotlinx.coroutines.android)
    api(libs.kotlinx.coroutines.play.services)
    api(libs.material)
    api(libs.play.services.oss.licenses)
    api(libs.timber)
    coreLibraryDesugaring(libs.desugar)
    ksp(libs.androidx.room.compiler)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit.ktx)
    androidTestImplementation(libs.androidx.room.testing)
    androidTestImplementation(libs.androidx.test.runner)
}
