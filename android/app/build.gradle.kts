// The Android app is only a NativeActivity: all the game is the C code in
// ../../src, built by the CMakeLists.txt at the root of the repository.
plugins {
    id("com.android.application")
}

android {
    namespace = "dev.timberheart.game"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "dev.timberheart.game"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
        externalNativeBuild {
            cmake {
                arguments += "-DTIMBERHEART_TESTS=OFF"
                // Build with a local copy of raylib instead of downloading it:
                // ./gradlew assembleDebug -PraylibSourceDir=/path/to/raylib
                providers.gradleProperty("raylibSourceDir").orNull?.let {
                    arguments += "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$it"
                }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.31.6"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Signed with the debug key so the APK can be installed for
            // testing. Use your own key before publishing to a store.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}
