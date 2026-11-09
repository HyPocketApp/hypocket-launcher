plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

// The commit an APK was built from, shown in test reports so a report can be
// matched to the code. "unknown" outside a git checkout.
val gitCommit: String = runCatching {
    providers.exec {
        commandLine("git", "rev-parse", "--short=8", "HEAD")
        isIgnoreExitValue = true
    }.standardOutput.asText.get().trim()
}.getOrNull()?.takeIf { it.isNotEmpty() } ?: "unknown"

android {
    namespace = "io.github.hypocketapp.launcher"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "io.github.hypocketapp.launcher"
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        buildConfigField("String", "GIT_COMMIT", "\"$gitCommit\"")

        ndk {
            // Same ABIs as the engine, so the app is never installed on a
            // device where the native engine is missing.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    signingConfigs {
        // A fixed debug key shared by everyone, so test APKs from any machine
        // install as updates of each other. It is public and only for debug
        // builds: official releases are signed with a private key that is
        // never committed.
        getByName("debug") {
            storeFile = file("debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    packaging {
        jniLibs {
            // Extract native libraries to disk: the CPU translator
            // (libbox64.so) is started as a separate process from there.
            useLegacyPackaging = true
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(project(":core"))
    implementation(project(":blocks"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons.core)
    implementation(libs.compose.ui.tooling.preview)
    debugImplementation(libs.compose.ui.tooling)
}
