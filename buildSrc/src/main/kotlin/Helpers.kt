
import com.android.build.VariantOutput
import com.android.build.gradle.AbstractAppExtension
import com.android.build.gradle.BaseExtension
import com.android.build.gradle.internal.api.ApkVariantOutputImpl
import org.gradle.api.JavaVersion
import org.gradle.api.Project
import org.gradle.api.plugins.ExtensionAware
import org.gradle.kotlin.dsl.dependencies
import org.gradle.kotlin.dsl.getByName
import org.jetbrains.kotlin.gradle.dsl.KotlinJvmOptions
import java.util.*

const val lifecycleVersion = "2.2.0"

private val Project.android get() = extensions.getByName<BaseExtension>("android")

private val flavorRegex = "(assemble|generate)\\w*(Release|Debug)".toRegex()
val Project.currentFlavor get() = gradle.startParameter.taskRequests.toString().let { task ->
    flavorRegex.matchEntire(task)?.groupValues?.get(2)?.toLowerCase(Locale.ROOT) ?: "debug".also {
        println("Warning: No match found for $task")
    }
}

fun Project.setupCommon() {
    android.apply {
        compileSdkVersion(29)
        defaultConfig {
            minSdkVersion(21)
            targetSdkVersion(29)
            testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        }
        val javaVersion = JavaVersion.VERSION_1_8
        compileOptions {
            sourceCompatibility = javaVersion
            targetCompatibility = javaVersion
        }
        (this as ExtensionAware).extensions.getByName<KotlinJvmOptions>("kotlinOptions").jvmTarget =
                javaVersion.toString()
    }

    dependencies {
        add("testImplementation", "junit:junit:4.13")
        add("androidTestImplementation", "androidx.test:runner:1.2.0")
        add("androidTestImplementation", "androidx.test.espresso:espresso-core:3.2.0")
    }
}

fun Project.setupCore() {
    setupCommon()
    android.apply {
        defaultConfig {
            versionCode = 5000650
            versionName = "5.0.6-nightly"
        }
        compileOptions.isCoreLibraryDesugaringEnabled = true
    }
    dependencies.add("coreLibraryDesugaring", "com.android.tools:desugar_jdk_libs:1.0.5")
}

private val abiCodes = mapOf("armeabi-v7a" to 1, "arm64-v8a" to 2, "x86" to 3, "x86_64" to 4)
fun Project.setupApp() {
    setupCore()

    android.apply {
        defaultConfig.resConfigs(listOf("ar", "es", "fa", "fr", "ja", "ko", "ru", "tr", "zh-rCN", "zh-rTW"))
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
        packagingOptions {
            exclude("**/*.kotlin_*")
        }
        splits.abi {
            isEnable = true
            isUniversalApk = true
        }
    }

    dependencies.add("implementation", project(":core"))

    if (currentFlavor == "release") (android as AbstractAppExtension).applicationVariants.all {
        for (output in outputs) {
            abiCodes[(output as ApkVariantOutputImpl).getFilter(VariantOutput.ABI)]?.let { offset ->
                output.versionCodeOverride = versionCode + offset
            }
        }
    }
}
