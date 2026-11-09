package io.github.hypocketapp.blocks

import android.content.Context
import io.github.hypocketapp.engine.DirectDisplay
import io.github.hypocketapp.engine.DisplayStatus
import io.github.hypocketapp.engine.GuestGame
import io.github.hypocketapp.engine.MemoryUsage
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

    /** Asks the game to make [recipe] once; it does if the player carries the inputs. */
    fun craft(recipe: Recipe) = synchronized(io) {
        for (k in 0 until 2) {
            val input = recipe.inputs.getOrNull(k)
            io.put(OFF_CRAFT + k * 2, (input?.first?.ordinal ?: NONE).toByte())
            io.put(OFF_CRAFT + k * 2 + 1, (input?.second ?: 0).toByte())
        }
        io.put(OFF_CRAFT + 4, recipe.output.ordinal.toByte())
        io.put(OFF_CRAFT + 5, recipe.count.toByte())
        io.putInt(OFF_CRAFTS, ++crafts)
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
                counts = List(COUNTS) { io.getShort(OFF_COUNTS + it * 2).toInt() },
                finished = io.getInt(OFF_FINISHED) != 0,
            )
        }

    /**
     * Runs the game at [width] x [height] until [stop] is called. Blocking:
     * call it from a background thread. Without [display] the game reads its
     * frames back into [frameFile].
     */
    fun run(width: Int, height: Int, fpsLimit: Int, display: DirectDisplay?, options: BlocksOptions = BlocksOptions()): BlocksReport {
        val env = buildMap {
            put("HYPOCKET_INPUT_PATH", ioFile.absolutePath)
            put("HYPOCKET_FRAME_PATH", frameFile.absolutePath)
            if (options.dynamicResolution) put("HYPOCKET_BLOCKS_DYNAMIC", "1")
            if (options.heavy) put("HYPOCKET_BLOCKS_HEAVY", "1")
            if (options.benchmarkFrames > 0) put("HYPOCKET_BLOCKS_BENCH", "1")
            if (options.creative) put("HYPOCKET_BLOCKS_CREATIVE", "1")
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

    /** Asks the game to exit, and ends it if it does not within a few seconds. */
    fun stop() {
        stopping = true
        synchronized(io) { io.putInt(OFF_QUIT, 1) }
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
        const val IO_SIZE = 256
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
        const val OFF_COUNTS = 128
        const val COUNTS = 16
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
    /** Blocks the player carries, by index into the building palette. */
    val counts: List<Int> = emptyList(),
    /** The game saved and exited normally. */
    val finished: Boolean = false,
)

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
    /** Unlimited blocks, nothing collected (quick games, and worlds made creative). */
    val creative: Boolean = true,
    /** Wandering creatures outside heavy mode (heavy mode always has 300). */
    val creatures: Int = 0,
    /** More environment for the game (Mesa and Box64 options under test). */
    val extraEnv: Map<String, String> = emptyMap(),
    /** More environment for the VirGL server (its options under test). */
    val serverEnv: Map<String, String> = emptyMap(),
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
