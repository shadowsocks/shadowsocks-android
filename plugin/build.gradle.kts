plugins {
    id("com.android.library")
    id("com.vanniktech.maven.publish")
    kotlin("android")
    id("kotlin-parcelize")
}

setupCommon()

android {
    defaultConfig {
        versionCode = findProperty("VERSION_CODE").toString().toInt()
        versionName = findProperty("VERSION_NAME").toString()
    }
}

mavenPublish.targets.getByName("uploadArchives") {
    releaseRepositoryUrl = "https://oss.sonatype.org/service/local/staging/deploy/maven2/"
    snapshotRepositoryUrl = "https://oss.sonatype.org/content/repositories/snapshots/"
    repositoryUsername = findProperty("NEXUS_USERNAME").toString()
    repositoryPassword = findProperty("NEXUS_PASSWORD").toString()
}

dependencies {
    api(kotlin("stdlib-jdk8"))
    api("androidx.core:core-ktx:1.3.2")
    // https://android-developers.googleblog.com/2019/07/android-q-beta-5-update.html
    api("androidx.drawerlayout:drawerlayout:1.1.1")
    api("com.google.android.material:material:1.3.0")
}
