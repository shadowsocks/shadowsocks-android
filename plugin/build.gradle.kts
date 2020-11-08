plugins {
    id(Plugins.androidLibrary)
    id(Plugins.kotlinAndroid)
    id(Plugins.kotlinExtensions)
    id("com.vanniktech.maven.publish")
}

setupCommon().run {
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
    api(Libs.core)
    api(Libs.drawerLayout)
    api(Libs.material)
}
