
import com.android.build.VariantOutput
import com.android.build.api.dsl.CommonExtension
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

const val lifecycleVersion = "2.5.1"

private val Project.android get() = extensions.getByName<BaseExtension>("android")
private val BaseExtension.lint get() = (this as CommonExtension<*, *, *, *>).lint

private val flavorRegex = "(assemble|generate)\\w*(Release|Debug)".toRegex()
val Project.currentFlavor get() = gradle.startParameter.taskRequests.toString().let { task ->
    flavorRegex.find(task)?.groupValues?.get(2)?.toLowerCase(Locale.ROOT) ?: "debug".also {
        println("Warning: No match found for $task")
    }
}

fun Project.setupCommon() {
    android.apply {
        buildToolsVersion("33.0.1")
        compileSdkVersion(33)
        defaultConfig {
            minSdk = 23
            targetSdk = 33
            testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        }
        val javaVersion = JavaVersion.VERSION_11
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
        (this as ExtensionAware).extensions.getByName<KotlinJvmOptions>("kotlinOptions").jvmTarget =
                javaVersion.toString()
    }

    dependencies {
        add("testImplementation", "junit:junit:4.13.2")
        add("androidTestImplementation", "androidx.test:runner:1.5.2")
        add("androidTestImplementation", "androidx.test.espresso:espresso-core:3.5.1")
    }
}

fun Project.setupCore() {
    setupCommon()
    android.apply {
        defaultConfig {
            versionCode = 5030350
            versionName = "5.3.3-nightly"
        }
        compileOptions.isCoreLibraryDesugaringEnabled = true
        lint.apply {
            disable += "BadConfigurationProvider"
            warning += "RestrictedApi"
            disable += "UseAppTint"
        }
        ndkVersion = "25.1.8937393"
    }
    dependencies.add("coreLibraryDesugaring", "com.android.tools:desugar_jdk_libs:2.0.2")
}

private val abiCodes = mapOf("armeabi-v7a" to 1, "arm64-v8a" to 2, "x86" to 3, "x86_64" to 4)
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

    if (currentFlavor == "release") (android as AbstractAppExtension).applicationVariants.all {
        for (output in outputs) {
            abiCodes[(output as ApkVariantOutputImpl).getFilter(VariantOutput.ABI)]?.let { offset ->
                output.versionCodeOverride = versionCode + offset
            }
        }
    }
}
