import com.android.build.api.dsl.CommonExtension
import com.android.build.gradle.BaseExtension
import org.gradle.api.JavaVersion
import org.gradle.api.Project
import org.gradle.kotlin.dsl.dependencies
import org.gradle.kotlin.dsl.getByName
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinAndroidProjectExtension
import java.util.Locale

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
        compileSdkVersion(36)
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
    }
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
