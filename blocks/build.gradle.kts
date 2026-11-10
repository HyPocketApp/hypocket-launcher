// HyPocket Blocks: an original block game that runs on HyPocket's engine.
// Kept apart from the launcher so it can grow on its own (and move to its own
// repository later): it depends on the engine's public game interface
// (GuestGame, DirectDisplay) and on nothing in :app or :core.
plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "io.github.hypocketapp.blocks"
    compileSdk = 36

    defaultConfig {
        minSdk = 28
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }
}

/**
 * The game's sound files (blocks/assets/audio, see its README) go into the
 * APK's assets as blocks-media/; only the Ogg files, not the notes beside
 * them. BlocksMedia copies them where the game reads them.
 */
abstract class PackBlocksMediaTask : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val audioDir: DirectoryProperty

    @get:OutputDirectory
    abstract val assetsDir: DirectoryProperty

    @get:Inject
    abstract val files: FileSystemOperations

    @TaskAction
    fun pack() {
        files.sync {
            from(audioDir) { include("**/*.ogg") }
            into(assetsDir.dir("blocks-media"))
        }
    }
}

val packBlocksMedia = tasks.register<PackBlocksMediaTask>("packBlocksMedia") {
    audioDir.set(layout.projectDirectory.dir("assets/audio"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(packBlocksMedia, PackBlocksMediaTask::assetsDir)
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(project(":engine"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)

    testImplementation(libs.junit)
}
