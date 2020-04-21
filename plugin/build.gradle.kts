plugins {
    id("com.android.library")
    id("com.vanniktech.maven.publish")
    kotlin("android")
    kotlin("android.extensions")
}

setupCommon()

android {
    defaultConfig {
        versionCode = findProperty("VERSION_CODE").toString().toInt()
        versionName = findProperty("VERSION_NAME").toString()
    }
}

androidExtensions.isExperimental = true

mavenPublish.targets.getByName("uploadArchives") {
    releaseRepositoryUrl = "https://oss.sonatype.org/service/local/staging/deploy/maven2/"
    snapshotRepositoryUrl = "https://oss.sonatype.org/content/repositories/snapshots/"
    repositoryUsername = findProperty("NEXUS_USERNAME").toString()
    repositoryPassword = findProperty("NEXUS_PASSWORD").toString()
}

dependencies {
    api(kotlin("stdlib-jdk8", rootProject.extra.get("kotlinVersion").toString()))
    api("androidx.core:core-ktx:1.2.0")
    // https://android-developers.googleblog.com/2019/07/android-q-beta-5-update.html
    api("androidx.drawerlayout:drawerlayout:1.1.0-beta01")
    api("com.google.android.material:material:1.1.0")
}
