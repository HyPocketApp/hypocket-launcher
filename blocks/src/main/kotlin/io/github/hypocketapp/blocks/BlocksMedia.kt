package io.github.hypocketapp.blocks

import android.content.res.AssetManager
import java.io.File

/**
 * The game's sound files: Ogg Vorbis files packed into the APK's assets under
 * [ASSET_DIR] (from blocks/assets/audio, see blocks/build.gradle.kts). The
 * game reads files, not assets, so they are copied to [dir] before it starts,
 * and its folder goes to the game as HYPOCKET_BLOCKS_MEDIA ("Sound files" in
 * blocks.c).
 */
class BlocksMedia(private val assets: AssetManager, private val dir: File) {

    /**
     * Copies the files that are new or changed, removes those no longer in
     * the APK, and returns the folder, or null when there are none.
     */
    fun prepare(): File? {
        val wanted = list(ASSET_DIR)
        if (wanted.isEmpty()) {
            dir.deleteRecursively()
            return null
        }
        for (path in wanted) {
            val target = File(dir, path)
            // Ogg files are stored uncompressed, so their size is known; if
            // not, copy them every time.
            val size = runCatching { assets.openFd("$ASSET_DIR/$path").use { it.length } }.getOrDefault(-1L)
            if (size >= 0 && target.length() == size) continue
            target.parentFile?.mkdirs()
            assets.open("$ASSET_DIR/$path").use { input -> target.outputStream().use { input.copyTo(it) } }
        }
        dir.walkBottomUp().forEach { file ->
            val path = file.relativeTo(dir).path
            if (file.isFile && path !in wanted) file.delete()
            if (file.isDirectory && file != dir && file.list().isNullOrEmpty()) file.delete()
        }
        return dir
    }

    /** Files under [path] in the assets, relative to [ASSET_DIR]. */
    private fun list(path: String): List<String> =
        assets.list(path).orEmpty().flatMap { name ->
            val child = "$path/$name"
            val inside = list(child)
            if (inside.isEmpty()) listOf(child.removePrefix("$ASSET_DIR/")) else inside
        }

    companion object {
        const val ASSET_DIR = "blocks-media"
    }
}
