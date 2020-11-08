plugins {
    id(Plugins.androidApplication)
    id(Plugins.kotlinAndroid)
    id(Plugins.kotlinExtensions)
    id("com.google.android.gms.oss-licenses-plugin")
    id("com.google.gms.google-services")
    id("com.google.firebase.crashlytics")
}

setupApp().run {
    defaultConfig.applicationId = "com.github.shadowsocks"
}

dependencies {

    implementation(Libs.browser)
    implementation(*Libs.camera)
    implementation(Libs.constraintLayout)
    implementation(*Libs.lifecycle)
    implementation(Libs.zxing)
    implementation("com.google.firebase:firebase-ads:19.5.0")
    implementation("com.google.mlkit:barcode-scanning:16.0.3")
    implementation("com.takisoft.preferencex:preferencex-simplemenu:1.1.0")
    implementation("com.twofortyfouram:android-plugin-api-for-locale:1.0.4")
    implementation("me.zhanghai.android.fastscroll:library:1.1.4")
}
