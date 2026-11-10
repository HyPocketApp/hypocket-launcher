package io.github.hypocketapp.blocks

import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * A world's mods (blocks/game/mods.c): one folder each in <save>.mods/,
 * which the game reads when the world starts. Added from a .zip file.
 */
class BlocksMods(private val world: BlocksWorld) {
    val directory: File get() = File(world.saveFile.path + SUFFIX)

    /** The mods' folder names, in the order the game loads them. */
    fun list(): List<String> =
        directory.listFiles()?.filter { it.isDirectory && !it.name.startsWith(".") }?.map { it.name }?.sorted().orEmpty()

    /**
     * Unpacks a mod from [zip]: its files at the top of the archive, or in
     * one folder there. Returns the mod's name. Entries that would land
     * outside the mod's folder, and archives too large, are refused.
     */
    fun add(zip: InputStream, fallbackName: String): String {
        val staging = File(directory, ".adding")
        staging.deleteRecursively()
        staging.mkdirs()
        try {
            var total = 0L
            var entries = 0
            ZipInputStream(zip.buffered()).use { input ->
                while (true) {
                    val entry = input.nextEntry ?: break
                    if (++entries > MAX_ENTRIES) throw IOException("too many files in the mod")
                    val target = File(staging, entry.name)
                    if (!target.canonicalPath.startsWith(staging.canonicalPath + File.separator)) {
                        throw IOException("a file in the mod points outside it: ${entry.name}")
                    }
                    if (entry.isDirectory) {
                        target.mkdirs()
                        continue
                    }
                    target.parentFile?.mkdirs()
                    target.outputStream().use { out ->
                        val buffer = ByteArray(16 * 1024)
                        while (true) {
                            val n = input.read(buffer)
                            if (n < 0) break
                            total += n
                            if (total > MAX_BYTES) throw IOException("the mod is larger than ${MAX_BYTES / (1 shl 20)} MB")
                            out.write(buffer, 0, n)
                        }
                    }
                }
            }
            // A single folder at the top is the mod itself.
            val top = staging.listFiles().orEmpty().filter { !it.name.startsWith(".") && it.name != "__MACOSX" }
            val root = top.singleOrNull()?.takeIf { it.isDirectory } ?: staging
            val name = cleanName(if (root == staging) fallbackName else root.name)
            val target = File(directory, name)
            target.deleteRecursively()
            if (!root.renameTo(target)) throw IOException("could not put the mod in place")
            return name
        } finally {
            staging.deleteRecursively()
        }
    }

    fun remove(name: String) {
        if (name.isEmpty() || name.startsWith(".") || name.contains('/')) return
        File(directory, name).deleteRecursively()
    }

    companion object {
        const val SUFFIX = ".mods"
        private const val MAX_BYTES = 64L shl 20
        private const val MAX_ENTRIES = 2000

        /** A folder name from a file name: letters, digits, "-" and "_" only. */
        fun cleanName(name: String): String =
            name.removeSuffix(".zip").replace(Regex("[^A-Za-z0-9_-]"), "_").trim('_').take(48).ifEmpty { "mod" }
    }
}
