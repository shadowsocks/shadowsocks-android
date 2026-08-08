import com.android.build.api.dsl.CommonExtension
import com.android.build.gradle.BaseExtension
import org.gradle.api.JavaVersion
import org.gradle.api.Project
import org.gradle.kotlin.dsl.dependencies
import org.gradle.kotlin.dsl.getByName
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinAndroidProjectExtension
import java.time.LocalDateTime
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter

private val Project.android get() = extensions.getByName<BaseExtension>("android")
private val BaseExtension.lint get() = (this as CommonExtension<*, *, *, *, *, *>).lint

val Project.currentFlavor get() = gradle.startParameter.taskNames.let { tasks ->
    when {
        tasks.any { it.contains("Release", ignoreCase = true) } -> "release"
        tasks.any { it.contains("Debug", ignoreCase = true) } -> "debug"
        else -> "debug".also {
            println("Warning: No match found for $tasks")
        }
    }
}

fun Project.setupCommon() {
    val javaVersion = JavaVersion.VERSION_11
    android.apply {
        compileSdkVersion(36)
        defaultConfig {
            minSdk = 23
            targetSdk = 36
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
            // Build timestamp (UTC) as version code; hour precision keeps it
            // below Play's 2100000000 cap until year 2100.
            versionCode = LocalDateTime.now(ZoneOffset.UTC)
                .format(DateTimeFormatter.ofPattern("yyyyMMddHH")).toInt()
            versionName = "5.3.6"
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
                packagingOptions.doNotStrip("**/libsslocal.so")
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
