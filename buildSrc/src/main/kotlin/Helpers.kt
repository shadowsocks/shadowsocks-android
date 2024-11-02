import com.android.build.api.dsl.CommonExtension
import com.android.build.gradle.BaseExtension
import org.gradle.api.JavaVersion
import org.gradle.api.Project
import org.gradle.kotlin.dsl.dependencies
import org.gradle.kotlin.dsl.getByName
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinAndroidProjectExtension
import java.util.Locale

const val lifecycleVersion = "2.8.7"

private val Project.android get() = extensions.getByName<BaseExtension>("android")
private val BaseExtension.lint get() = (this as CommonExtension<*, *, *, *, *, *>).lint

private val flavorRegex = "(assemble|generate)\\w*(Release|Debug)".toRegex()
val Project.currentFlavor get() = gradle.startParameter.taskRequests.toString().let { task ->
    flavorRegex.find(task)?.groupValues?.get(2)?.lowercase(Locale.ROOT) ?: "debug".also {
        println("Warning: No match found for $task")
    }
}

fun Project.setupCommon() {
    val javaVersion = JavaVersion.VERSION_11
    android.apply {
        compileSdkVersion(35)
        defaultConfig {
            minSdk = 23
            targetSdk = 35
            testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        }
        compileOptions {
            sourceCompatibility = javaVersion
            targetCompatibility = javaVersion
        }
        lint.apply {
            warning += "ExtraTranslation"
            warning += "ImpliedQuantity"
            informational += "MissingQuantity"
            informational += "MissingTranslation"
        }
    }
    extensions.getByName<KotlinAndroidProjectExtension>("kotlin").compilerOptions.jvmTarget
        .set(JvmTarget.fromTarget(javaVersion.toString()))

    dependencies {
        add("testImplementation", "junit:junit:4.13.2")
        add("androidTestImplementation", "androidx.test:runner:1.6.2")
        add("androidTestImplementation", "androidx.test.espresso:espresso-core:3.6.1")
    }
}

fun Project.setupCore() {
    setupCommon()
    android.apply {
        defaultConfig {
            versionCode = 5030450
            versionName = "5.3.4-nightly"
        }
        compileOptions.isCoreLibraryDesugaringEnabled = true
        lint.apply {
            disable += "BadConfigurationProvider"
            warning += "RestrictedApi"
            disable += "UseAppTint"
        }
        buildFeatures.buildConfig = true
        ndkVersion = "27.2.12479018"
    }
    dependencies.add("coreLibraryDesugaring", "com.android.tools:desugar_jdk_libs:2.1.2")
}

fun Project.setupApp() {
    setupCore()

    android.apply {
        defaultConfig.resourceConfigurations.addAll(listOf(
            "ar",
            "de",
            "es",
            "fa",
            "fr",
            "ja",
            "ko",
            "ru",
            "tr",
            "uk",
            "zh-rCN",
            "zh-rTW",
        ))
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
        lint.disable += "RemoveWorkManagerInitializer"
        packagingOptions.jniLibs.useLegacyPackaging = true
        splits.abi {
            isEnable = true
            isUniversalApk = true
        }
    }

    dependencies.add("implementation", project(":core"))
}
