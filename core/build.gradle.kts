plugins {
    id("com.android.library")
    kotlin("android")
    kotlin("android.extensions")
    kotlin("kapt")
}

val rootExt = rootProject.extra

android {
    compileSdkVersion(rootExt.get("compile_sdk_version") as Int)
    defaultConfig {
        minSdkVersion(rootExt.get("min_sdk_version") as Int)
        targetSdkVersion(rootExt.get("sdk_version") as Int)
        versionCode = rootExt.get("version_code") as Int
        versionName = rootExt.get("version_name") as String
        consumerProguardFiles("proguard-rules.pro")
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            ndkBuild {
                abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
                arguments("-j${Runtime.getRuntime().availableProcessors()}")
            }
        }
        javaCompileOptions.annotationProcessorOptions {
            arguments = mapOf(
                    "room.incremental" to "true",
                    "room.schemaLocation" to "$projectDir/schemas"
            )
        }
    }
    val javaVersion = rootExt.get("java_version") as JavaVersion
    compileOptions {
        isCoreLibraryDesugaringEnabled = true
        sourceCompatibility = javaVersion
        targetCompatibility = javaVersion
    }
    kotlinOptions {
        jvmTarget = javaVersion.toString()
        freeCompilerArgs = freeCompilerArgs + listOf(
                "-Xuse-experimental=kotlin.ExperimentalUnsignedTypes",
                "-Xuse-experimental=kotlinx.coroutines.ObsoleteCoroutinesApi",
                "-Xuse-experimental=kotlinx.coroutines.ExperimentalCoroutinesApi"
        )
    }
    externalNativeBuild {
        ndkBuild { 
            path("src/main/jni/Android.mk")
        }
    }
    sourceSets {
        getByName("androidTest").assets.srcDirs.plus(files("$projectDir/schemas"))
    }
}

androidExtensions.isExperimental = true

dependencies {
    val coroutinesVersion = "1.3.5"
    val roomVersion = "2.2.5"
    val workVersion = "2.3.4"
    val lifecycleVersion = rootExt.get("lifecycle_version") as String
    val desugarLibsVersion = rootExt.get("desugar_libs_version") as String
    val junitVersion = rootExt.get("junit_version") as String
    val androidTestVersion = rootExt.get("android_test_version") as String
    val androidEspressoVersion = rootExt.get("android_espresso_version") as String

    api(project(":plugin"))
    api("androidx.fragment:fragment-ktx:1.2.4")
    api("androidx.lifecycle:lifecycle-common-java8:$lifecycleVersion")
    api("androidx.lifecycle:lifecycle-livedata-core-ktx:$lifecycleVersion")
    api("androidx.preference:preference:1.1.0")
    api("androidx.room:room-runtime:$roomVersion")
    api("androidx.work:work-runtime-ktx:$workVersion")
    api("androidx.work:work-gcm:$workVersion")
    api("com.google.android.gms:play-services-oss-licenses:17.0.0")
    api("com.google.code.gson:gson:2.8.6")
    api("com.google.firebase:firebase-analytics-ktx:17.3.0")
    api("com.google.firebase:firebase-config:19.1.3")
    api("com.google.firebase:firebase-config-ktx:19.1.3")
    api("com.google.firebase:firebase-crashlytics:17.0.0-beta04")
    api("com.jakewharton.timber:timber:4.7.1")
    api("dnsjava:dnsjava:3.0.2")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:$coroutinesVersion")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-play-services:$coroutinesVersion")
    api("org.connectbot.jsocks:jsocks:1.0.0")
    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:$desugarLibsVersion")
    kapt("androidx.room:room-compiler:$roomVersion")
    testImplementation("junit:junit:$junitVersion")
    androidTestImplementation("androidx.room:room-testing:$roomVersion")
    androidTestImplementation("androidx.test:runner:$androidTestVersion")
    androidTestImplementation("androidx.test.espresso:espresso-core:$androidEspressoVersion")
    androidTestImplementation("androidx.test.ext:junit-ktx:1.1.1")
}
