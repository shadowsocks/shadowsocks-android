import org.jetbrains.kotlin.config.KotlinCompilerVersion

plugins {
    id("com.android.library")
    kotlin("android")
    kotlin("android.extensions")
    id("com.vanniktech.maven.publish")
}

val rootExt = rootProject.extra

android {
    compileSdkVersion(rootExt.get("compile_sdk_version") as Int)
    defaultConfig {
        minSdkVersion(rootExt.get("min_sdk_version") as Int)
        targetSdkVersion(rootExt.get("sdk_version") as Int)
        versionCode = rootExt.get("version_code") as Int
        versionName = rootExt.get("version_name") as String
        testInstrumentationRunner = "androidx.testgetRepositoryPassword().runner.AndroidJUnitRunner"
    }
    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
        }
    }
    val javaVersion = rootExt.get("java_version") as JavaVersion
    compileOptions {
        sourceCompatibility = javaVersion
        targetCompatibility = javaVersion
    }
    kotlinOptions {
        jvmTarget = javaVersion.toString()
    }
}

androidExtensions.isExperimental = true

mavenPublish {
    targets {
        getByName("uploadArchives") {
            releaseRepositoryUrl = "https://oss.sonatype.org/service/local/staging/deploy/maven2/"
            snapshotRepositoryUrl = "https://oss.sonatype.org/content/repositories/snapshots/"
            repositoryUsername = findProperty("NEXUS_USERNAME") as? String ?: ""
            repositoryPassword = findProperty("NEXUS_PASSWORD") as? String ?: ""
        }
    }
}

dependencies {
    val junitVersion = rootExt.get("junit_version") as String
    val androidTestVersion = rootExt.get("android_test_version") as String
    val androidEspressoVersion = rootExt.get("android_espresso_version") as String

    api(kotlin("stdlib-jdk8", KotlinCompilerVersion.VERSION))
    api("androidx.core:core-ktx:1.2.0")
    api("androidx.drawerlayout:drawerlayout:1.1.0-beta01")  // https://android-developers.googleblog.com/2019/07/android-q-beta-5-update.html
    api("com.google.android.material:material:1.1.0")
    testImplementation("junit:junit:$junitVersion")
    androidTestImplementation("androidx.test:runner:$androidTestVersion")
    androidTestImplementation("androidx.test.espresso:espresso-core:$androidEspressoVersion")
}

repositories {
    mavenCentral()
}
