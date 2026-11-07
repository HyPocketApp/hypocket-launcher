import java.io.ByteArrayOutputStream
import java.io.OutputStream
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
 * kept as a git submodule pinned in third_party/box64.
 *
 * Box64's Android build only runs x86-64 *Android* programs, so we build the
 * Linux (glibc) version for arm64 instead and ship a minimal ARM64 glibc
 * runtime with it. At run time the app starts glibc's dynamic loader, which
 * loads Box64, which runs the x86-64 Linux program.
 *
 * Android only extracts and lets apps execute files named lib*.so from the
 * APK, so every file is renamed: see GLIBC_RUNTIME below and TranslatorProbe.
 *
 * The glibc runtime is built from source with a patch for Android's seccomp
 * filter (see third_party/glibc-android). Only libgcc_s comes from the cross
 * compiler.
 *
 * This needs an aarch64 Linux cross compiler (on Debian/Ubuntu:
 * `apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu bison gawk`).
 * Without it the build continues and the app reports that the translator is
 * unavailable.
 */

/** glibc runtime files and the names they get in the APK. */
val GLIBC_RUNTIME = mapOf(
    "ld-linux-aarch64.so.1" to "libglibc_ld.so",
    "libc.so.6" to "libglibc_c.so",
    "libm.so.6" to "libglibc_m.so",
    "libresolv.so.2" to "libglibc_resolv.so",
    "libdl.so.2" to "libglibc_dl.so",
    "libpthread.so.0" to "libglibc_pthread.so",
    "librt.so.1" to "libglibc_rt.so",
    "libutil.so.1" to "libglibc_util.so",
    "libgcc_s.so.1" to "libglibc_gcc_s.so",
)

/** Builds glibc with HyPocket's Android patch, using third_party/glibc-android/build.sh. */
abstract class BuildGlibcTask @Inject constructor(
    private val execOperations: ExecOperations,
) : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val scriptDir: DirectoryProperty

    @get:Input
    abstract val crossPrefix: Property<String>

    @get:Internal
    abstract val workDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun build() {
        val out = outputDir.get().asFile
        out.deleteRecursively()
        out.mkdirs()
        if (!commandExists("${crossPrefix.get()}gcc")) {
            logger.warn("HyPocket: no aarch64 cross compiler, so glibc is not built.")
            return
        }
        execOperations.exec {
            commandLine(
                scriptDir.get().file("build.sh").asFile.absolutePath,
                workDir.get().asFile.absolutePath,
                out.absolutePath,
            )
        }
    }
    private fun commandExists(command: String): Boolean = try {
        execOperations.exec {
            commandLine(command, "--version")
            standardOutput = OutputStream.nullOutputStream()
            errorOutput = OutputStream.nullOutputStream()
        }.exitValue == 0
    } catch (e: Exception) {
        false
    }
}

abstract class BuildBox64Task @Inject constructor(
    private val execOperations: ExecOperations,
) : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val sourceDir: DirectoryProperty

    @get:Input
    abstract val cmakeDir: Property<String>

    @get:Input
    abstract val crossPrefix: Property<String>

    @get:Input
    abstract val runtimeFiles: MapProperty<String, String>

    /** Output of [BuildGlibcTask]. */
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val glibcDir: DirectoryProperty

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
        val cc = "${crossPrefix.get()}gcc"
        val abiDir = jniLibsDir.get().asFile.resolve("arm64-v8a")
        abiDir.deleteRecursively()

        if (!commandExists(cc)) {
            logger.warn(
                "HyPocket: '$cc' not found, so Box64 is not built and the translator test " +
                    "will report it as unavailable. See engine/build.gradle.kts.",
            )
            return
        }

        val cmake = "${cmakeDir.get()}/bin/cmake"
        val ninja = "${cmakeDir.get()}/bin/ninja"
        val work = workDir.get().asFile
        execOperations.exec {
            commandLine(
                cmake, "-S", source.absolutePath, "-B", work.absolutePath, "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=$ninja",
                "-DCMAKE_SYSTEM_NAME=Linux",
                "-DCMAKE_SYSTEM_PROCESSOR=aarch64",
                "-DCMAKE_C_COMPILER=$cc",
                "-DCMAKE_CXX_COMPILER=${crossPrefix.get()}g++",
                "-DCMAKE_ASM_COMPILER=$cc",
                "-DARM64=ON",
                "-DARM_DYNAREC=ON",
                // Android kernels can report incoherent si_info on SIGSEGV.
                "-DBAD_SIGNAL=ON",
                "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
                "-DNO_LIB_INSTALL=ON",
                "-DNO_CONF_INSTALL=ON",
            )
        }
        execOperations.exec { commandLine(ninja, "-C", work.absolutePath, "box64") }

        abiDir.mkdirs()
        work.resolve("box64").copyTo(abiDir.resolve("libbox64.so"), overwrite = true)
        runtimeFiles.get().forEach { (name, apkName) ->
            locateRuntimeFile(cc, name).copyTo(abiDir.resolve(apkName), overwrite = true)
        }
    }

    private fun commandExists(command: String): Boolean = try {
        execOperations.exec {
            commandLine(command, "--version")
            standardOutput = OutputStream.nullOutputStream()
            errorOutput = OutputStream.nullOutputStream()
        }.exitValue == 0
    } catch (e: Exception) {
        false
    }

    /**
     * Prefers the patched glibc. Falls back to the cross compiler's sysroot,
     * which is only expected for libgcc_s (not part of glibc).
     */
    private fun locateRuntimeFile(cc: String, name: String): File {
        val patched = glibcDir.get().file(name).asFile
        if (patched.isFile) return patched
        check(name == "libgcc_s.so.1") {
            "Patched glibc is missing $name. Stock glibc cannot run on Android."
        }
        val output = ByteArrayOutputStream()
        execOperations.exec {
            commandLine(cc, "-print-file-name=$name")
            standardOutput = output
        }
        val file = File(output.toString().trim()).canonicalFile
        check(file.isFile) { "Could not find $name for $cc (got '$file')" }
        return file
    }
}

val crossPrefixSetting = providers.gradleProperty("hypocket.aarch64CrossPrefix").orElse("aarch64-linux-gnu-")

val buildGlibc = tasks.register<BuildGlibcTask>("buildGlibc") {
    description = "Builds glibc for arm64 with the patch for Android's seccomp filter."
    scriptDir.set(layout.projectDirectory.dir("third_party/glibc-android"))
    crossPrefix.set(crossPrefixSetting)
    workDir.set(layout.buildDirectory.dir("glibc-android/work"))
    outputDir.set(layout.buildDirectory.dir("glibc-android/out"))
}

val buildBox64 = tasks.register<BuildBox64Task>("buildBox64") {
    description = "Builds the Box64 CPU translator and its glibc runtime for arm64-v8a."
    sourceDir.set(layout.projectDirectory.dir("third_party/box64"))
    cmakeDir.set(androidComponents.sdkComponents.sdkDirectory.map { "${it.asFile.absolutePath}/cmake/$CMAKE_VERSION" })
    crossPrefix.set(crossPrefixSetting)
    runtimeFiles.set(GLIBC_RUNTIME)
    glibcDir.set(buildGlibc.flatMap { it.outputDir })
    workDir.set(layout.buildDirectory.dir("box64/cmake-glibc"))
    jniLibsDir.set(layout.buildDirectory.dir("box64/jniLibs"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.jniLibs?.addGeneratedSourceDirectory(buildBox64, BuildBox64Task::jniLibsDir)
    }
}
