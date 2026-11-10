package io.github.hypocketapp.blocks

import android.content.Context
import io.github.hypocketapp.blocks.ui.BuildBlock
import io.github.hypocketapp.engine.DirectDisplay
import io.github.hypocketapp.engine.DisplayStatus
import io.github.hypocketapp.engine.GuestGame
import io.github.hypocketapp.engine.MemoryUsage
import io.github.hypocketapp.engine.RunLog
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import kotlin.concurrent.thread

/**
 * Runs HyPocket Blocks (blocks/game), the original block game, through
 * [GuestGame]: the same path a real game takes (Box64, desktop OpenGL 4.1 and
 * VirGL, with frames shown by [DirectDisplay]).
 *
 * Touch input and the game's statistics go through a small shared file (the
 * layout is SharedIo in blocks.c). Input setters can be called from any
 * thread; [run] blocks until the game exits.
 */
class BlocksSession(context: Context) : AutoCloseable {
    private val game = GuestGame(context)
    private val media = BlocksMedia(context.assets, File(context.filesDir, "blocks-media"))

    /** Frames read back by the game when direct display is not possible. */
    val frameFile: File = File(game.cacheDir, "hypocket-frame.bin")
    private val ioFile = File(game.cacheDir, "hypocket-blocks-io.bin")
    private val audioFile = File(game.cacheDir, "hypocket-blocks-audio.bin")
    private val io: MappedByteBuffer

    private var yaw = 0f
    private var pitch = 0f
    private var breaks = 0
    private var places = 0

    @Volatile
    private var process: Process? = null

    @Volatile
    private var stopping = false

    init {
        frameFile.delete()
        RandomAccessFile(ioFile, "rw").use { file ->
            file.setLength(0)
            file.setLength(IO_SIZE.toLong())
            io = file.channel.map(FileChannel.MapMode.READ_WRITE, 0, IO_SIZE.toLong())
        }
        io.order(ByteOrder.LITTLE_ENDIAN)
        io.putInt(OFF_VERSION, 1)
        io.putInt(OFF_VOLUME, 100)
        io.putInt(OFF_MAGIC, MAGIC)
    }

    /** Joystick: [x] strafes (right is positive), [y] walks (forward is positive), both -1..1. */
    fun setMove(x: Float, y: Float) = synchronized(io) {
        io.putFloat(OFF_MOVE_X, x.coerceIn(-1f, 1f))
        io.putFloat(OFF_MOVE_Y, y.coerceIn(-1f, 1f))
    }

    /** Turns the camera by [dYaw] (right is positive) and [dPitch] (up is positive), in radians. */
    fun look(dYaw: Float, dPitch: Float) = synchronized(io) {
        yaw += dYaw
        // The game starts looking slightly down (-0.25) and clamps at ±1.5.
        pitch = (pitch + dPitch).coerceIn(-1.25f, 1.75f)
        io.putFloat(OFF_LOOK_YAW, yaw)
        io.putFloat(OFF_LOOK_PITCH, pitch)
    }

    /** Turns the camera continuously, in radians per second (a controller stick). */
    fun setLookRate(yaw: Float, pitch: Float) = synchronized(io) {
        io.putFloat(OFF_LOOK_RATE_YAW, yaw)
        io.putFloat(OFF_LOOK_RATE_PITCH, pitch)
    }

    fun setJump(held: Boolean) = synchronized(io) { io.putInt(OFF_JUMP, if (held) 1 else 0) }

    /** Breaks the block under the crosshair. */
    fun breakBlock() = synchronized(io) { io.putInt(OFF_BREAKS, ++breaks) }

    /** Places a block next to the one under the crosshair. */
    fun placeBlock() = synchronized(io) { io.putInt(OFF_PLACES, ++places) }

    private var crafts = 0

    /** Asks the game to make [recipe] once; it does if the player carries the inputs and the tool, near the workstation. */
    fun craft(recipe: Recipe) = synchronized(io) {
        for (k in 0 until 2) {
            val input = recipe.inputs.getOrNull(k)
            io.put(OFF_CRAFT + k * 2, (input?.first?.ordinal ?: NONE).toByte())
            io.put(OFF_CRAFT + k * 2 + 1, (input?.second ?: 0).toByte())
        }
        io.put(OFF_CRAFT + 4, recipe.output.ordinal.toByte())
        io.put(OFF_CRAFT + 5, recipe.count.toByte())
        io.put(OFF_CRAFT + 6, (recipe.station?.ordinal ?: NONE).toByte())
        io.put(OFF_CRAFT + 7, (recipe.tool?.ordinal ?: NONE).toByte())
        io.putInt(OFF_CRAFTS, ++crafts)
    }

    /** Holds the world still (the pause menu); it is still drawn, at 30 fps. */
    fun setPaused(paused: Boolean) = synchronized(io) { io.putInt(OFF_PAUSED, if (paused) 1 else 0) }

    /** Applies the in-game settings the game itself handles; they take effect on the next frame. */
    fun applySettings(settings: BlocksSettings.Values) = synchronized(io) {
        io.putInt(OFF_VIEW, settings.viewDistance)
        io.putInt(OFF_VOLUME, settings.volumePercent)
        io.putInt(OFF_FPS_SETTING, if (settings.fpsLimit == 0) -1 else settings.fpsLimit)
        io.putInt(OFF_SCALE_MAX, settings.scalePercent)
    }

    /** Chooses the block [placeBlock] places: an index into the game's building palette. */
    fun selectBlock(index: Int) = synchronized(io) { io.putInt(OFF_SELECTED, index) }

    /** What the game reports while it runs. */
    val stats: BlocksStats
        get() = synchronized(io) {
            BlocksStats(
                ready = io.getInt(OFF_READY) != 0,
                frames = io.getInt(OFF_FRAMES),
                fps = io.getFloat(OFF_FPS),
                frameMillis = io.getFloat(OFF_FRAME_MS),
                triangles = io.getInt(OFF_TRIANGLES),
                chunks = io.getInt(OFF_CHUNKS),
                buildMillis = io.getFloat(OFF_BUILD_MS),
                width = io.getInt(OFF_WIDTH),
                height = io.getInt(OFF_HEIGHT),
                scalePercent = io.getInt(OFF_SCALE),
                creatures = io.getInt(OFF_CREATURES),
                saves = io.getInt(OFF_SAVES),
                clockMinutes = io.getInt(OFF_CLOCK),
                day = io.getInt(OFF_DAY),
                counts = List(COUNTS) { io.getShort(OFF_ALL_COUNTS + it * 2).toInt() },
                finished = io.getInt(OFF_FINISHED) != 0,
                healthTenths = io.getInt(OFF_HEALTH),
                deaths = io.getInt(OFF_DEATHS),
                hurts = io.getInt(OFF_HURTS),
                nightNear = io.getInt(OFF_NIGHT_NEAR),
                beacons = io.getInt(OFF_BEACONS),
                beaconsLit = io.getInt(OFF_BEACONS + 4),
                beaconAngle = io.getInt(OFF_BEACONS + 8),
                beaconDistance = io.getInt(OFF_BEACONS + 12),
                goalDone = io.getInt(OFF_BEACONS + 16) != 0,
                playSeconds = io.getInt(OFF_BEACONS + 20),
                doneSeconds = io.getInt(OFF_BEACONS + 24),
                mistPercent = io.getInt(OFF_BEACONS + 28),
                aimingBeacon = io.getInt(OFF_BEACONS + 32) != 0,
                storyFlags = io.getInt(OFF_STORY),
                grabPercent = io.getInt(OFF_STORY + 4),
                notesOpened = io.getInt(OFF_STORY + 8),
                noteId = io.getInt(OFF_STORY + 12),
                notesFound = io.getInt(OFF_STORY + 16),
                takes = io.getInt(OFF_STORY + 20),
                foodTenths = io.getInt(OFF_SURVIVAL),
                warmthTenths = io.getInt(OFF_SURVIVAL + 4),
                survivalFlags = io.getInt(OFF_SURVIVAL + 8),
                digPercent = io.getInt(OFF_SURVIVAL + 12),
                picks = io.getInt(OFF_SURVIVAL + 16),
                picked = io.getInt(OFF_SURVIVAL + 20),
                needs = io.getInt(OFF_SURVIVAL + 24),
                needed = io.getInt(OFF_SURVIVAL + 28),
                eats = io.getInt(OFF_SURVIVAL + 32),
            )
        }

    /**
     * Runs the game at [width] x [height] until [stop] is called. Blocking:
     * call it from a background thread. Without [display] the game reads its
     * frames back into [frameFile]. The output also goes to [log] as it comes.
     */
    fun run(
        width: Int,
        height: Int,
        fpsLimit: Int,
        display: DirectDisplay?,
        options: BlocksOptions = BlocksOptions(),
        log: RunLog? = null,
    ): BlocksReport {
        val env = buildMap {
            put("HYPOCKET_INPUT_PATH", ioFile.absolutePath)
            put("HYPOCKET_FRAME_PATH", frameFile.absolutePath)
            media.prepare()?.let { put("HYPOCKET_BLOCKS_MEDIA", it.absolutePath) }
            if (options.dynamicResolution) put("HYPOCKET_BLOCKS_DYNAMIC", "1")
            if (options.heavy) put("HYPOCKET_BLOCKS_HEAVY", "1")
            if (options.benchmarkFrames > 0) put("HYPOCKET_BLOCKS_BENCH", "1")
            if (options.creative) put("HYPOCKET_BLOCKS_CREATIVE", "1")
            if (options.peaceful) put("HYPOCKET_BLOCKS_PEACEFUL", "1")
            when (options.dayMode) {
                DayMode.CYCLE -> {}
                DayMode.FAST -> put("HYPOCKET_BLOCKS_DAY_SECONDS", DayMode.FAST_DAY_SECONDS.toString())
                DayMode.ALWAYS_DAY -> put("HYPOCKET_BLOCKS_TIME_LOCK", "0.5")
                DayMode.ALWAYS_NIGHT -> put("HYPOCKET_BLOCKS_TIME_LOCK", "0.0")
            }
            options.world?.let { world ->
                put("HYPOCKET_BLOCKS_SAVE", world.saveFile.absolutePath)
                put("HYPOCKET_BLOCKS_CHUNKS", world.chunks.toString())
                put("HYPOCKET_BLOCKS_SEED", world.seed.toString())
            }
            if (options.creatures > 0) put("HYPOCKET_BLOCKS_CREATURES", options.creatures.toString())
            putAll(options.extraEnv)
        }
        if (options.native) return runNative(width, height, fpsLimit, env, options, log)
        // Benchmarks measure what the phone can do, not the screen's refresh rate.
        val benchmarkServerEnv =
            if (options.benchmarkFrames > 0) mapOf("HYPOCKET_DISPLAY_UNTHROTTLED" to "1", "HYPOCKET_PROFILE" to "1") else emptyMap()
        val result = game.run(
            GuestGame.Launch(
                asset = ASSET,
                args = listOf(width.toString(), height.toString(), fpsLimit.toString()) +
                    (if (options.benchmarkFrames > 0) listOf(options.benchmarkFrames.toString()) else emptyList()),
                env = env,
                serverEnv = benchmarkServerEnv + options.serverEnv,
                display = display,
                audioFile = audioFile,
                preciseFences = options.extraEnv["HYPOCKET_FENCES"] != "0",
                timeoutSeconds = MAX_SESSION_SECONDS,
                log = log,
                onStart = { started ->
                    process = started
                    if (stopping) started.destroy()
                },
            ),
        )
        process = null
        val text = result.output
        return BlocksReport(
            // The game marks the shared file (and says "blocks: ok") when it ran to the end.
            // The output alone is not enough: the engine keeps only its start.
            status = if (result.status == DisplayStatus.PASSED && !stats.finished && !text.contains("blocks: ok")) {
                DisplayStatus.FAILED
            } else {
                result.status
            },
            stats = stats,
            renderer = Regex("""blocks: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
            exitCode = result.exitCode,
            seconds = result.seconds,
            direct = display != null,
            directInfo = result.directInfo,
            memory = result.memory,
            audioInfo = result.audioInfo,
            options = options,
            output = text,
            serverOutput = result.serverOutput,
        )
    }

    /**
     * The native game ([BlocksOptions.native]) in this process: it draws into
     * the surface given to [NativeGame.setSurface] and writes its output to
     * a file, read for the report when it ends.
     */
    private fun runNative(
        width: Int,
        height: Int,
        fpsLimit: Int,
        env: Map<String, String>,
        options: BlocksOptions,
        log: RunLog?,
    ): BlocksReport {
        val output = File(game.cacheDir, "hypocket-blocks-native.log")
        val started = System.nanoTime()
        val code = NativeGame.run(
            arrayOf(width.toString(), height.toString(), fpsLimit.toString()),
            env.map { (name, value) -> "$name=$value" }.toTypedArray(),
            output.absolutePath,
        )
        val seconds = (System.nanoTime() - started) / 1e9
        val text = runCatching { output.readText() }.getOrDefault("")
        log?.section("game (native)")
        text.lineSequence().forEach { log?.line(it) }
        return BlocksReport(
            status = if (code == 0 && (stats.finished || text.contains("blocks: ok"))) DisplayStatus.PASSED else DisplayStatus.FAILED,
            stats = stats,
            renderer = Regex("""blocks: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
            exitCode = code,
            seconds = seconds,
            direct = false,
            directInfo = "native",
            audioInfo = Regex("""blocks: audio (\d+ Hz.*)""").find(text)?.groupValues?.get(1) ?: "no audio",
            options = options,
            output = text,
        )
    }

    /** Asks the game to exit, and ends it if it does not within a few seconds. */
    fun stop() {
        stopping = true
        synchronized(io) { io.putInt(OFF_QUIT, 1) }
        if (NativeGame.isAvailable) NativeGame.stop()
        thread(name = "blocks-stop", isDaemon = true) {
            Thread.sleep(STOP_GRACE_MILLIS)
            process?.destroyForcibly()
        }
    }

    override fun close() {
        stop()
        ioFile.delete()
    }

    private companion object {
        const val ASSET = "blocks-x86_64"
        const val MAX_SESSION_SECONDS = 4 * 60 * 60L
        const val STOP_GRACE_MILLIS = 3000L

        // Must match SharedIo in blocks/game/blocks.c.
        const val IO_SIZE = 512
        const val MAGIC = 0x49425048
        const val OFF_MAGIC = 0
        const val OFF_VERSION = 4
        const val OFF_MOVE_X = 8
        const val OFF_MOVE_Y = 12
        const val OFF_LOOK_YAW = 16
        const val OFF_LOOK_PITCH = 20
        const val OFF_JUMP = 24
        const val OFF_BREAKS = 28
        const val OFF_PLACES = 32
        const val OFF_QUIT = 36
        const val OFF_LOOK_RATE_YAW = 40
        const val OFF_LOOK_RATE_PITCH = 44
        const val OFF_SELECTED = 48
        const val OFF_CRAFTS = 52
        const val OFF_CRAFT = 56
        const val NONE = 255
        const val OFF_READY = 64
        const val OFF_FRAMES = 68
        const val OFF_FPS = 72
        const val OFF_FRAME_MS = 76
        const val OFF_TRIANGLES = 80
        const val OFF_CHUNKS = 84
        const val OFF_BUILD_MS = 88
        const val OFF_WIDTH = 92
        const val OFF_HEIGHT = 96
        const val OFF_SCALE = 100
        const val OFF_CREATURES = 104
        const val OFF_SAVES = 108
        const val OFF_CLOCK = 112
        const val OFF_DAY = 116
        const val OFF_FINISHED = 120
        const val OFF_ALL_COUNTS = 256
        val COUNTS = BuildBlock.entries.size
        const val OFF_HEALTH = 160
        const val OFF_DEATHS = 164
        const val OFF_HURTS = 168
        const val OFF_NIGHT_NEAR = 172
        const val OFF_PAUSED = 176
        const val OFF_VIEW = 180
        const val OFF_VOLUME = 184
        const val OFF_FPS_SETTING = 188
        const val OFF_SCALE_MAX = 192
        const val OFF_BEACONS = 196
        const val OFF_STORY = 232
        const val OFF_SURVIVAL = 352
    }
}

data class BlocksStats(
    /** True once the world is built and the first frame was shown. */
    val ready: Boolean = false,
    val frames: Int = 0,
    /** Frames per second over the last second. */
    val fps: Float = 0f,
    /** Time the game spent on one frame (CPU and GPU), over the last second. */
    val frameMillis: Float = 0f,
    val triangles: Int = 0,
    val chunks: Int = 0,
    /** Time to generate and mesh the world. */
    val buildMillis: Float = 0f,
    val width: Int = 0,
    val height: Int = 0,
    /** Share of the width and height the 3D scene is drawn at (dynamic resolution). */
    val scalePercent: Int = 100,
    /** Creatures drawn in the last frame. */
    val creatures: Int = 0,
    /** Times the game wrote the world to its save file. */
    val saves: Int = 0,
    /** Time of day in the game, in minutes after midnight. */
    val clockMinutes: Int = 0,
    /** Days passed in the world, from 1 (0 until the game reports it). */
    val day: Int = 0,
    /** Blocks and items the player carries, by index into the palette ([BuildBlock]). */
    val counts: List<Int> = emptyList(),
    /** The game saved and exited normally. */
    val finished: Boolean = false,
    /** The player's health in tenths of a point: 0 to [MAX_HEALTH_TENTHS]. */
    val healthTenths: Int = MAX_HEALTH_TENTHS,
    /** Times the player fainted and woke up at the start. */
    val deaths: Int = 0,
    /** Times the player was hurt (it goes up by one each time). */
    val hurts: Int = 0,
    /** Night creatures within 24 blocks. */
    val nightNear: Int = 0,
    /** Beacons in the world (the goal) and how many are lit. */
    val beacons: Int = 0,
    val beaconsLit: Int = 0,
    /** Degrees from where the player looks to the nearest unlit beacon, right positive. */
    val beaconAngle: Int = 0,
    /** Blocks to the nearest unlit beacon. */
    val beaconDistance: Int = 0,
    /** Every beacon is lit. */
    val goalDone: Boolean = false,
    /** Time played in this world, and when the last beacon was lit. */
    val playSeconds: Int = 0,
    val doneSeconds: Int = 0,
    /** How thick the mist is where the player stands. */
    val mistPercent: Int = 0,
    /** The crosshair is on an unlit beacon. */
    val aimingBeacon: Boolean = false,
    /** The story's state: [GRABBED], [SHELTERED], [AIMING_NOTE] and [AWAY] bits. */
    val storyFlags: Int = 0,
    /** While grabbed: how far the player has got free, 0 to 100. */
    val grabPercent: Int = 0,
    /** Goes up each time the player opens a note; [noteId] is the one opened last. */
    val notesOpened: Int = 0,
    val noteId: Int = 0,
    /** One bit per note found (see StoryNotes). */
    val notesFound: Int = 0,
    /** Times the player was dragged away by a night creature. */
    val takes: Int = 0,
    /** How full the player is, in tenths: 0 to [MAX_FOOD_TENTHS]. */
    val foodTenths: Int = MAX_FOOD_TENTHS,
    /** How warm the player is, in tenths: 0 to [MAX_WARMTH_TENTHS]. */
    val warmthTenths: Int = MAX_WARMTH_TENTHS,
    /** Survival: the [NEAR_KILN], [NEAR_STOVE], [NEAR_CAMPFIRE], [COLD] and [WARMING] bits. */
    val survivalFlags: Int = 0,
    /** How far the aimed block is from breaking, 0 to 100 (0: not started). */
    val digPercent: Int = 0,
    /** Goes up each time something is picked up or hunted; [picked] is the last ([BuildBlock] index). */
    val picks: Int = 0,
    val picked: Int = 0,
    /** Goes up each time something needs a tool the player lacks; [needed] is that tool ([BuildBlock] index). */
    val needs: Int = 0,
    val needed: Int = 0,
    /** Goes up each time the player eats. */
    val eats: Int = 0,
) {
    val grabbed: Boolean get() = storyFlags and GRABBED != 0
    val sheltered: Boolean get() = storyFlags and SHELTERED != 0
    val aimingNote: Boolean get() = storyFlags and AIMING_NOTE != 0
    val away: Boolean get() = storyFlags and AWAY != 0
    val aimingPickup: Boolean get() = storyFlags and AIMING_PICKUP != 0
    val cold: Boolean get() = survivalFlags and COLD != 0
    val warming: Boolean get() = survivalFlags and WARMING != 0

    companion object {
        const val MAX_HEALTH_TENTHS = 100

        // Must match STORY_* in blocks.c.
        const val GRABBED = 1
        const val SHELTERED = 2
        const val AIMING_NOTE = 4
        const val AWAY = 8
        const val AIMING_PICKUP = 16

        const val MAX_FOOD_TENTHS = 100
        const val MAX_WARMTH_TENTHS = 100

        // Must match SURVIVAL_* in blocks.c.
        const val NEAR_KILN = 1
        const val NEAR_STOVE = 2
        const val NEAR_CAMPFIRE = 4
        const val COLD = 8
        const val WARMING = 16
    }
}

/** What the game does beyond the defaults. */
data class BlocksOptions(
    /** Lower the 3D resolution while the GPU cannot keep up with the FPS limit. */
    val dynamicResolution: Boolean = false,
    /** A world four times larger, a longer view and 300 wandering creatures. */
    val heavy: Boolean = false,
    /** Above 0: a benchmark of this many frames with a fixed camera path. */
    val benchmarkFrames: Int = 0,
    /** A saved world to load and save; without one, a fresh world nobody keeps. */
    val world: BlocksWorld? = null,
    /** How time passes: the normal day, a fast one, or held at noon or midnight (for testing). */
    val dayMode: DayMode = DayMode.CYCLE,
    /** No night creatures (they never attack); falls still hurt. */
    val peaceful: Boolean = false,
    /** Unlimited blocks, nothing collected (quick games, and worlds made creative). */
    val creative: Boolean = true,
    /** Wandering creatures outside heavy mode (heavy mode always has 300). */
    val creatures: Int = 0,
    /** More environment for the game (Mesa and Box64 options under test). */
    val extraEnv: Map<String, String> = emptyMap(),
    /** More environment for the VirGL server (its options under test). */
    val serverEnv: Map<String, String> = emptyMap(),
    /**
     * Run the game natively (libblocks.so) instead of the emulated x86-64
     * program. Native is much faster; the emulated one tests the engine.
     */
    val native: Boolean = false,
)

/** How time passes in a game. Holding the time never changes the time kept in the world's save. */
enum class DayMode {
    /** A full day every 15 minutes. */
    CYCLE,

    /** A full day every [FAST_DAY_SECONDS] seconds, to see a day pass quickly. */
    FAST,

    /** Held at noon. */
    ALWAYS_DAY,

    /** Held at midnight, to test lamps and the night. */
    ALWAYS_NIGHT,
    ;

    companion object {
        const val FAST_DAY_SECONDS = 120

        fun fromName(name: String?): DayMode = entries.firstOrNull { it.name == name } ?: CYCLE
    }
}

data class BlocksReport(
    val status: DisplayStatus,
    /** The game's last statistics. */
    val stats: BlocksStats,
    val renderer: String? = null,
    val exitCode: Int? = null,
    val seconds: Double = 0.0,
    val direct: Boolean = false,
    val directInfo: String = "",
    val memory: MemoryUsage? = null,
    /** What the app's audio output saw. */
    val audioInfo: String = "",
    val options: BlocksOptions = BlocksOptions(),
    val output: String = "",
    val serverOutput: String = "",
)
