import javax.inject.Inject

val CMAKE_VERSION = "3.31.6"

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "io.github.hypocketapp.engine"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        minSdk = 28
        consumerProguardFiles("consumer-rules.pro")

        ndk {
            // arm64-v8a is the real target. x86_64 is only kept so the app
            // can run on desktop emulators during development.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = CMAKE_VERSION
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

/*
 * Box64 (MIT, https://github.com/ptitSeb/box64) is an upstream dependency,
 * kept as a git submodule pinned in third_party/box64. It builds an
 * executable, which AGP cannot package from externalNativeBuild, so this task
 * builds it with the NDK and ships it as lib/arm64-v8a/libbox64.so. Android
 * only lets apps execute files from their native library directory.
 */
abstract class BuildBox64Task @Inject constructor(
    private val execOperations: ExecOperations,
) : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val sourceDir: DirectoryProperty

    @get:Input
    abstract val ndkDir: Property<String>

    @get:Input
    abstract val cmakeDir: Property<String>

    @get:Input
    abstract val minSdk: Property<Int>

    @get:OutputDirectory
    abstract val workDir: DirectoryProperty

    @get:OutputDirectory
    abstract val jniLibsDir: DirectoryProperty

    @TaskAction
    fun build() {
        val source = sourceDir.get().asFile
        check(source.resolve("CMakeLists.txt").exists()) {
            "Box64 sources are missing. Run: git submodule update --init --recursive"
        }
        val cmake = "${cmakeDir.get()}/bin/cmake"
        val ninja = "${cmakeDir.get()}/bin/ninja"
        val work = workDir.get().asFile

        execOperations.exec {
            commandLine(
                cmake, "-S", source.absolutePath, "-B", work.absolutePath, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=$ninja",
                "-DCMAKE_TOOLCHAIN_FILE=${ndkDir.get()}/build/cmake/android.toolchain.cmake",
                "-DANDROID_ABI=arm64-v8a",
                "-DANDROID_PLATFORM=android-${minSdk.get()}",
                "-DANDROID=ON",
                "-DARM64=ON",
                "-DARM_DYNAREC=ON",
                "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
                "-DNO_LIB_INSTALL=ON",
                "-DNO_CONF_INSTALL=ON",
            )
        }
        execOperations.exec {
            commandLine(ninja, "-C", work.absolutePath, "box64")
        }

        val abiDir = jniLibsDir.get().asFile.resolve("arm64-v8a")
        abiDir.mkdirs()
        work.resolve("box64").copyTo(abiDir.resolve("libbox64.so"), overwrite = true)
    }
}


val buildBox64 = tasks.register<BuildBox64Task>("buildBox64") {
    description = "Builds the Box64 CPU translator for arm64-v8a."
    sourceDir.set(layout.projectDirectory.dir("third_party/box64"))
    ndkDir.set(androidComponents.sdkComponents.ndkDirectory.map { it.asFile.absolutePath })
    cmakeDir.set(androidComponents.sdkComponents.sdkDirectory.map { "${it.asFile.absolutePath}/cmake/$CMAKE_VERSION" })
    minSdk.set(28)
    workDir.set(layout.buildDirectory.dir("box64/cmake"))
    jniLibsDir.set(layout.buildDirectory.dir("box64/jniLibs"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.jniLibs?.addGeneratedSourceDirectory(buildBox64, BuildBox64Task::jniLibsDir)
    }
}
