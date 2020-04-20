import com.android.build.OutputFile
import java.util.regex.Pattern

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("kapt")
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
}

fun getCurrentFlavor(): String {
    val task = gradle.startParameter.taskRequests.toString()
    val matcher = Pattern.compile("(assemble|generate)\\w*(Release|Debug)").matcher(task)
    return if (matcher.find()) {
        matcher.group(2).toLowerCase()
    } else {
        println("Warning: No match found for $task")
        "debug"
    }
}

val rootExt = rootProject.extra

android {
    compileSdkVersion(rootExt.get("compile_sdk_version") as Int)
    defaultConfig {
        applicationId = "com.github.shadowsocks.tv"
        minSdkVersion(rootExt.get("min_sdk_version") as Int)
        targetSdkVersion(rootExt.get("sdk_version") as Int)
        versionCode = rootExt.get("version_code") as Int
        versionName = rootExt.get("version_name") as String
        @Suppress("UNCHECKED_CAST")
        resConfigs(rootExt.get("res_configs") as List<String>)
    }
    buildTypes {
        getByName("debug") {
            isPseudoLocalesEnabled = true
        }
        getByName("release") {
            isShrinkResources = true
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
        }
    }
    val javaVersion = rootExt.get("java_version") as JavaVersion
    compileOptions {
        coreLibraryDesugaringEnabled = true
        sourceCompatibility = javaVersion
        targetCompatibility = javaVersion
    }
    kotlinOptions {
        jvmTarget = javaVersion.toString()
    }
    packagingOptions {
        exclude("**/*.kotlin_*")
    }
    splits {
        abi {
            isEnable = true
            isUniversalApk = true
        }
    }
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs.plus(File(project(":core").buildDir, "intermediates/bundles/${getCurrentFlavor()}/jni"))
        }
    }
}

dependencies {
    val desugarLibsVersion = rootExt.get("desugar_libs_version") as String

    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:$desugarLibsVersion")
    implementation(project(":core"))
    implementation(fileTree(mapOf("dir" to "libs", "include" to listOf("*.jar"))))
    implementation("androidx.leanback:leanback-preference:1.1.0-alpha03")
}

val abiCodes = mapOf("armeabi-v7a" to 1, "arm64-v8a" to 2, "x86" to 3, "x86_64" to 4)

if (getCurrentFlavor() == "release") {
    android.applicationVariants.all {
        outputs.map { baseOutput ->
            baseOutput as com.android.build.gradle.internal.api.ApkVariantOutputImpl
        }.forEach { output ->
            val offset = abiCodes[output.getFilter(OutputFile.ABI)]
            if (offset != null) {
                output.versionCodeOverride = versionCode + offset
            }
        }
    }
}

