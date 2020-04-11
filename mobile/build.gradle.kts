import com.android.build.OutputFile
import java.util.regex.Pattern

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("android.extensions")
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("io.fabric")
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
        applicationId = "com.github.shadowsocks"
        minSdkVersion(rootExt.get("min_sdk_version") as Int)
        targetSdkVersion(rootExt.get("sdk_version") as Int)
        versionCode = rootExt.get("version_code") as Int
        versionName = rootExt.get("version_name") as String
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
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
            proguardFile(getDefaultProguardFile("proguard-android.txt"))
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
androidExtensions.isExperimental = true

dependencies {
    val lifecycleVersion = rootExt.get("lifecycle_version") as String
    val desugarLibsVersion = rootExt.get("desugar_libs_version") as String
    val junitVersion = rootExt.get("junit_version") as String
    val androidTestVersion = rootExt.get("android_test_version") as String
    val androidEspressoVersion = rootExt.get("android_espresso_version") as String

    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:$desugarLibsVersion")
    implementation(project(":core"))
    implementation("androidx.browser:browser:1.2.0")
    implementation("androidx.constraintlayout:constraintlayout:2.0.0-beta4")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:$lifecycleVersion")
    implementation("com.google.android.gms:play-services-vision:20.0.0")
    implementation("com.google.firebase:firebase-ads:19.0.1")
    implementation("com.google.zxing:core:3.4.0")
    implementation("com.takisoft.preferencex:preferencex-simplemenu:1.1.0")
    implementation("com.twofortyfouram:android-plugin-api-for-locale:1.0.4")
    implementation("me.zhanghai.android.fastscroll:library:1.1.2")
    implementation("xyz.belvi.mobilevision:barcodescanner:2.0.3")
    testImplementation("junit:junit:$junitVersion")
    androidTestImplementation("androidx.test:runner:$androidTestVersion")
    androidTestImplementation("androidx.test.espresso:espresso-core:$androidEspressoVersion")
}
repositories {
    mavenCentral()
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
