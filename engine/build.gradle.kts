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
 * Graphics: Box64 forwards the program's EGL/GL calls to Mesa built for arm64
 * glibc with only the virgl driver (third_party/mesa-guest). Mesa sends them
 * to a VirGL server built for Android (third_party/virgl-android), which runs
 * them on the phone's own OpenGL ES driver.
 *
 * Android only extracts and lets apps execute files named lib*.so from the
 * APK, so guest files are renamed: see GUEST_RUNTIME below and GuestRuntime.kt.
 *
 * The glibc runtime is built from source with a patch for Android's seccomp
 * filter (see third_party/glibc-android).
 *
 * This needs an aarch64 Linux cross compiler and a few build tools; see
 * CONTRIBUTING.md. Without the cross compiler the build continues and the app
 * reports that the translator is unavailable.
 */

/** Guest (arm64 glibc) runtime files and the names they get in the APK. */
val GUEST_RUNTIME = mapOf(
    "ld-linux-aarch64.so.1" to "libglibc_ld.so",
    "libc.so.6" to "libglibc_c.so",
    "libm.so.6" to "libglibc_m.so",
    "libresolv.so.2" to "libglibc_resolv.so",
    "libdl.so.2" to "libglibc_dl.so",
    "libpthread.so.0" to "libglibc_pthread.so",
    "librt.so.1" to "libglibc_rt.so",
    "libutil.so.1" to "libglibc_util.so",
    "libgcc_s.so.1" to "libglibc_gcc_s.so",
    "libstdc++.so.6" to "libglibc_stdcxx.so",
    "libEGL.so.1" to "libmesa_egl.so",
    "libGLESv2.so.2" to "libmesa_glesv2.so",
    "libgallium-26.2.6.so" to "libmesa_gallium.so",
    "libdrm.so.2" to "libmesa_drm.so",
    "libexpat.so.1" to "libmesa_expat.so",
    "libz.so.1" to "libmesa_z.so",
)

/** Files the cross compiler provides; everything else must come from a build script. */
val FROM_CROSS_COMPILER = setOf("libgcc_s.so.1", "libstdc++.so.6")

/** Runs one of the third_party/<name>/build.sh scripts. */
abstract class RunBuildScriptTask @Inject constructor(
    private val execOperations: ExecOperations,
) : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val scriptDir: DirectoryProperty

    @get:Input
    abstract val extraArgs: ListProperty<String>

    /** The script is skipped (empty output) when this command is missing. */
    @get:Input
    abstract val requiredCommand: Property<String>

    @get:Internal
    abstract val workDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun build() {
        val out = outputDir.get().asFile
        out.deleteRecursively()
        out.mkdirs()
        if (!commandExists(requiredCommand.get())) {
            logger.warn("HyPocket: '${requiredCommand.get()}' not found, skipping ${scriptDir.get().asFile.name}.")
            return
        }
        execOperations.exec {
            commandLine(
                listOf(
                    scriptDir.get().file("build.sh").asFile.absolutePath,
                    workDir.get().asFile.absolutePath,
                    out.absolutePath,
                ) + extraArgs.get(),
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

    @get:Input
    abstract val crossCompilerFiles: SetProperty<String>

    /** Outputs of the guest build scripts (glibc, Mesa), renamed into the APK. */
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val guestLibDirs: ConfigurableFileCollection

    /** Outputs of the Android build scripts, copied into the APK as they are. */
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val androidLibDirs: ConfigurableFileCollection

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
        androidLibDirs.files.forEach { dir ->
            dir.listFiles()?.filter { it.isFile }?.forEach { it.copyTo(abiDir.resolve(it.name), overwrite = true) }
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

    /** Takes a file from the guest build outputs, or from the cross compiler. */
    private fun locateRuntimeFile(cc: String, name: String): File {
        guestLibDirs.files.map { it.resolve(name) }.firstOrNull { it.isFile }?.let { return it }
        check(name in crossCompilerFiles.get()) {
            "$name is missing from the guest build outputs. Stock glibc cannot run on Android."
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
val crossGcc = crossPrefixSetting.map { "${it}gcc" }

fun registerBuildScript(name: String, folder: String, describe: String, args: Provider<List<String>>) =
    tasks.register<RunBuildScriptTask>(name) {
        description = describe
        scriptDir.set(layout.projectDirectory.dir("third_party/$folder"))
        extraArgs.set(args)
        requiredCommand.set(crossGcc)
        workDir.set(layout.buildDirectory.dir("$folder/work"))
        outputDir.set(layout.buildDirectory.dir("$folder/out"))
    }

val buildGlibc = registerBuildScript(
    "buildGlibc", "glibc-android",
    "Builds glibc for arm64 with the patch for Android's seccomp filter.",
    provider { emptyList() },
)
val buildGuestMesa = registerBuildScript(
    "buildGuestMesa", "mesa-guest",
    "Builds Mesa (virgl driver only) for arm64 glibc.",
    provider { emptyList() },
)
val buildVirglServer = registerBuildScript(
    "buildVirglServer", "virgl-android",
    "Builds the VirGL render server for Android arm64.",
    androidComponents.sdkComponents.ndkDirectory.map { listOf(it.asFile.absolutePath, "28") },
)

val buildBox64 = tasks.register<BuildBox64Task>("buildBox64") {
    description = "Builds the Box64 CPU translator and packages it with its guest runtime for arm64-v8a."
    sourceDir.set(layout.projectDirectory.dir("third_party/box64"))
    cmakeDir.set(androidComponents.sdkComponents.sdkDirectory.map { "${it.asFile.absolutePath}/cmake/$CMAKE_VERSION" })
    crossPrefix.set(crossPrefixSetting)
    runtimeFiles.set(GUEST_RUNTIME)
    crossCompilerFiles.set(FROM_CROSS_COMPILER)
    guestLibDirs.from(buildGlibc.flatMap { it.outputDir }, buildGuestMesa.flatMap { it.outputDir })
    androidLibDirs.from(buildVirglServer.flatMap { it.outputDir })
    workDir.set(layout.buildDirectory.dir("box64/cmake-glibc"))
    jniLibsDir.set(layout.buildDirectory.dir("box64/jniLibs"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.jniLibs?.addGeneratedSourceDirectory(buildBox64, BuildBox64Task::jniLibsDir)
    }
}
