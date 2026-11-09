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
    /** Unlimited blocks and nothing to collect. Worlds made before this choice existed are creative. */
    val creative: Boolean = true,
    /** No creatures that attack. */
    val peaceful: Boolean = false,
    /** The hotbar's blocks, as indices into the building palette; empty for the default. */
    val hotbar: List<Int> = emptyList(),
) {
    val lastPlayedMillis: Long get() = if (saveFile.exists()) saveFile.lastModified() else createdMillis
    val sizeBytes: Long get() = if (saveFile.exists()) saveFile.length() else 0L

    /**
     * Seconds played when the last beacon was lit, read from the save's header
     * (HPBW version 4, see SaveHeader in blocks.c); null until then.
     */
    val completedSeconds: Int?
        get() = runCatching {
            val header = ByteArray(64)
            val read = saveFile.inputStream().use { it.read(header) }
            BlocksWorlds.completedSeconds(header.copyOf(read.coerceAtLeast(0)))
        }.getOrNull()

    /** A small picture of the world the game writes next to its save (see BlocksThumbnail). */
    val thumbnailFile: File get() = File(saveFile.path + ".thumb")
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
    fun create(
        name: String,
        chunks: Int,
        creative: Boolean = false,
        peaceful: Boolean = false,
        random: Random = Random.Default,
    ): BlocksWorld {
        require(chunks in SIZES) { "unsupported world size $chunks" }
        directory.mkdirs()
        val created = System.currentTimeMillis()
        val id = newId()
        val properties = Properties().apply {
            setProperty(KEY_NAME, cleanName(name))
            setProperty(KEY_CHUNKS, chunks.toString())
            setProperty(KEY_SEED, random.nextInt(0, 1_000_000).toString())
            setProperty(KEY_CREATED, created.toString())
            setProperty(KEY_CREATIVE, creative.toString())
            setProperty(KEY_PEACEFUL, peaceful.toString())
        }
        File(directory, id + META).outputStream().use { properties.store(it, "HyPocket Blocks world") }
        return checkNotNull(get(id))
    }

    /** Remembers how the player arranged the hotbar of world [id]. */
    fun saveHotbar(id: String, hotbar: List<Int>) {
        val properties = load(id) ?: return
        properties.setProperty(KEY_HOTBAR, hotbar.joinToString(","))
        store(id, properties)
    }

    private fun load(id: String): Properties? {
        if (!ID.matches(id)) return null
        val meta = File(directory, id + META)
        if (!meta.exists()) return null
        return Properties().takeIf { p -> runCatching { meta.inputStream().use { p.load(it) } }.isSuccess }
    }

    /** Writes next to the file and renames, so a crash never leaves half a file. */
    private fun store(id: String, properties: Properties) {
        val tmp = File(directory, "$id$META.tmp")
        tmp.outputStream().use { properties.store(it, "HyPocket Blocks world") }
        if (!tmp.renameTo(File(directory, id + META))) tmp.delete()
    }

    private fun newId(): String {
        var id = "w${System.currentTimeMillis()}"
        while (File(directory, id + META).exists()) id += "x"
        return id
    }

    private fun cleanName(name: String) = name.trim().ifEmpty { "World" }.take(MAX_NAME)

    /** Gives world [id] a new name. Returns the renamed world, or null if there is no such world. */
    fun rename(id: String, name: String): BlocksWorld? {
        val properties = load(id) ?: return null
        properties.setProperty(KEY_NAME, cleanName(name))
        store(id, properties)
        return get(id)
    }

    /**
     * Copies world [id], save and picture included, under [name]: a new world
     * that is changed apart from the first. Returns null if there is no such world.
     */
    fun duplicate(id: String, name: String): BlocksWorld? {
        val source = get(id) ?: return null
        val properties = load(id) ?: return null
        val copy = newId()
        properties.setProperty(KEY_NAME, cleanName(name))
        properties.setProperty(KEY_CREATED, System.currentTimeMillis().toString())
        if (source.saveFile.exists()) source.saveFile.copyTo(File(directory, copy + SAVE))
        if (source.thumbnailFile.exists()) source.thumbnailFile.copyTo(File(directory, "$copy$SAVE.thumb"))
        store(copy, properties)
        return get(copy)
    }

    /** Deletes a world and its save for good. */
    fun delete(id: String) {
        if (!ID.matches(id)) return
        File(directory, "$id$SAVE.tmp").delete()
        File(directory, "$id$SAVE.unreadable").delete()
        File(directory, "$id$SAVE.thumb").delete()
        File(directory, "$id$SAVE.thumb.tmp").delete()
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
            creative = properties.getProperty(KEY_CREATIVE)?.toBooleanStrictOrNull() ?: true,
            peaceful = properties.getProperty(KEY_PEACEFUL)?.toBooleanStrictOrNull() ?: false,
            hotbar = properties.getProperty(KEY_HOTBAR).orEmpty().split(",").mapNotNull { it.trim().toIntOrNull() },
        )
    }

    companion object {
        private const val SAVE_MAGIC = 0x57425048
        private const val GOAL_DONE = 2

        /** The completion time in a save's [header], or null if the goal is not done. */
        fun completedSeconds(header: ByteArray): Int? {
            if (header.size < 64) return null
            val b = java.nio.ByteBuffer.wrap(header).order(java.nio.ByteOrder.LITTLE_ENDIAN)
            if (b.getInt(0) != SAVE_MAGIC || b.getInt(4) < 4) return null
            if (b.getInt(56) and GOAL_DONE == 0) return null
            return b.getFloat(60).toInt()
        }

        /** Small, medium and large: 128, 256 and 384 blocks on each side. */
        val SIZES = listOf(8, 16, 24)
        const val MAX_NAME = 32

        private const val META = ".properties"
        private const val SAVE = ".hpw"
        private const val KEY_NAME = "name"
        private const val KEY_CHUNKS = "chunks"
        private const val KEY_SEED = "seed"
        private const val KEY_CREATED = "created"
        private const val KEY_CREATIVE = "creative"
        private const val KEY_HOTBAR = "hotbar"
        private const val KEY_PEACEFUL = "peaceful"
        private val ID = Regex("w[0-9]+x*")
    }
}
