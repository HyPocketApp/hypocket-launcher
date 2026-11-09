package io.github.hypocketapp.blocks

import java.io.File
import java.util.Properties
import kotlin.random.Random

/** A saved HyPocket Blocks world. */
data class BlocksWorld(
    val id: String,
    val name: String,
    /** Chunks of 16 blocks on each side. */
    val chunks: Int,
    /** Terrain of a world not played yet; a saved world keeps its own. */
    val seed: Int,
    val createdMillis: Long,
    /** The game's save file; missing until the world is first played. */
    val saveFile: File,
) {
    val lastPlayedMillis: Long get() = if (saveFile.exists()) saveFile.lastModified() else createdMillis
    val sizeBytes: Long get() = if (saveFile.exists()) saveFile.length() else 0L
}

/**
 * The player's HyPocket Blocks worlds, kept in [directory]: for each one the
 * game's save file (`<id>.hpw`, written by blocks.c) and a small properties
 * file with its name, size and seed (`<id>.properties`, written here).
 */
class BlocksWorlds(private val directory: File) {

    /** All worlds, the most recently played first. */
    fun list(): List<BlocksWorld> =
        directory.listFiles { file -> file.name.endsWith(META) }.orEmpty()
            .mapNotNull { read(it) }
            .sortedByDescending { it.lastPlayedMillis }

    fun get(id: String): BlocksWorld? =
        if (ID.matches(id)) File(directory, id + META).takeIf { it.exists() }?.let { read(it) } else null

    /** A new world; the game generates its terrain the first time it is played. */
    fun create(name: String, chunks: Int, random: Random = Random.Default): BlocksWorld {
        require(chunks in SIZES) { "unsupported world size $chunks" }
        directory.mkdirs()
        val created = System.currentTimeMillis()
        var id = "w$created"
        while (File(directory, id + META).exists()) id += "x"
        val properties = Properties().apply {
            setProperty(KEY_NAME, name.trim().ifEmpty { "World" }.take(MAX_NAME))
            setProperty(KEY_CHUNKS, chunks.toString())
            setProperty(KEY_SEED, random.nextInt(0, 1_000_000).toString())
            setProperty(KEY_CREATED, created.toString())
        }
        File(directory, id + META).outputStream().use { properties.store(it, "HyPocket Blocks world") }
        return checkNotNull(get(id))
    }

    /** Deletes a world and its save for good. */
    fun delete(id: String) {
        if (!ID.matches(id)) return
        File(directory, "$id$SAVE.tmp").delete()
        File(directory, "$id$SAVE.unreadable").delete()
        File(directory, id + SAVE).delete()
        File(directory, id + META).delete()
    }

    private fun read(meta: File): BlocksWorld? {
        val id = meta.name.removeSuffix(META)
        if (!ID.matches(id)) return null
        val properties = Properties()
        runCatching { meta.inputStream().use { properties.load(it) } }.getOrElse { return null }
        return BlocksWorld(
            id = id,
            name = properties.getProperty(KEY_NAME) ?: id,
            chunks = properties.getProperty(KEY_CHUNKS)?.toIntOrNull()?.takeIf { it in SIZES } ?: SIZES.first(),
            seed = properties.getProperty(KEY_SEED)?.toIntOrNull() ?: 0,
            createdMillis = properties.getProperty(KEY_CREATED)?.toLongOrNull() ?: meta.lastModified(),
            saveFile = File(directory, id + SAVE),
        )
    }

    companion object {
        /** Small, medium and large: 128, 256 and 384 blocks on each side. */
        val SIZES = listOf(8, 16, 24)
        const val MAX_NAME = 32

        private const val META = ".properties"
        private const val SAVE = ".hpw"
        private const val KEY_NAME = "name"
        private const val KEY_CHUNKS = "chunks"
        private const val KEY_SEED = "seed"
        private const val KEY_CREATED = "created"
        private val ID = Regex("w[0-9]+x*")
    }
}
