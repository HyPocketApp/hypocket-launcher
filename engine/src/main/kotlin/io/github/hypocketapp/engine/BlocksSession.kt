package io.github.hypocketapp.engine

import android.content.Context
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import kotlin.concurrent.thread

/**
 * Runs HyPocket Blocks (engine/guest-tests/blocks), the original test game,
 * through the same path a real game takes: Box64, desktop OpenGL 4.1 and
 * VirGL, with frames shown by [DirectDisplay].
 *
 * Touch input and the game's statistics go through a small shared file (the
 * layout is SharedIo in blocks.c). Input setters can be called from any
 * thread; [run] blocks until the game exits.
 */
class BlocksSession(context: Context) : AutoCloseable {
    private val runtime = GuestRuntime(context)

    /** Frames read back by the game when direct display is not possible. */
    val frameFile: File = File(runtime.cacheDir, "hypocket-frame.bin")
    private val ioFile = File(runtime.cacheDir, "hypocket-blocks-io.bin")
    private val audioFile = File(runtime.cacheDir, "hypocket-blocks-audio.bin")
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
            )
        }

    /**
     * Runs the game at [width] x [height] until [stop] is called. Blocking:
     * call it from a background thread. Without [display] the game reads its
     * frames back into [frameFile].
     */
    fun run(width: Int, height: Int, fpsLimit: Int, display: DirectDisplay?, options: BlocksOptions = BlocksOptions()): BlocksReport {
        if (!runtime.isAvailable || !VirglServer.binary(runtime).exists()) {
            return BlocksReport(DisplayStatus.NOT_AVAILABLE, stats)
        }
        val serverEnv = if (display != null) mapOf("HYPOCKET_DISPLAY_SOCKET" to display.socketName) else emptyMap()
        val presentEnv = if (display != null) mapOf("HYPOCKET_PRESENT" to "1") else emptyMap()
        val (server, failureLog) = VirglServer.start(runtime, serverEnv)
        if (server == null) return BlocksReport(DisplayStatus.SERVER_FAILED, stats, serverOutput = failureLog)
        // Without audio the game runs silent.
        val audio = AudioOutput.create(audioFile)
        val audioEnv = if (audio != null) mapOf("HYPOCKET_AUDIO_PATH" to audioFile.absolutePath) else emptyMap()
        val optionEnv = buildMap {
            if (options.dynamicResolution) put("HYPOCKET_BLOCKS_DYNAMIC", "1")
            if (options.heavy) put("HYPOCKET_BLOCKS_HEAVY", "1")
            if (options.benchmarkFrames > 0) put("HYPOCKET_BLOCKS_BENCH", "1")
            putAll(options.extraEnv)
        }

        MemorySampler().use { sampler -> server.use { audio.use {
            val result = runtime.runAsset(
                ASSET,
                extraEnv = server.guestEnvironment + mapOf(
                    "HYPOCKET_INPUT_PATH" to ioFile.absolutePath,
                    "HYPOCKET_FRAME_PATH" to frameFile.absolutePath,
                    "MESA_GL_VERSION_OVERRIDE" to "4.1",
                    "MESA_GLSL_VERSION_OVERRIDE" to "410",
                ) + presentEnv + audioEnv + optionEnv,
                args = listOf(width.toString(), height.toString(), fpsLimit.toString()) +
                    (if (options.benchmarkFrames > 0) listOf(options.benchmarkFrames.toString()) else emptyList()),
                timeoutSeconds = MAX_SESSION_SECONDS,
                onStart = { started ->
                    process = started
                    if (stopping) started.destroy()
                },
            )
            process = null
            val memory = sampler.finish()
            server.awaitExit(3)
            val text = result.output
            return BlocksReport(
                status = when {
                    !result.finished -> DisplayStatus.TIMED_OUT
                    result.blockedSyscall -> DisplayStatus.BLOCKED_SYSCALL
                    result.exitCode != 0 || !text.contains("blocks: ok") -> DisplayStatus.FAILED
                    else -> DisplayStatus.PASSED
                },
                stats = stats,
                renderer = Regex("""blocks: GL_RENDERER (.*)""").find(text)?.groupValues?.get(1),
                exitCode = result.exitCode,
                seconds = result.millis / 1000.0,
                direct = display != null,
                directInfo = display?.info.orEmpty(),
                memory = memory,
                audioInfo = audio?.info ?: "no audio stream",
                options = options,
                output = text,
                serverOutput = server.log,
            )
        } } }
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

        // Must match SharedIo in engine/guest-tests/blocks/blocks.c.
        const val IO_SIZE = 128
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
    /** Creatures drawn in the last frame (heavy mode). */
    val creatures: Int = 0,
)

/** What the game does beyond the defaults. */
data class BlocksOptions(
    /** Lower the 3D resolution while the GPU cannot keep up with the FPS limit. */
    val dynamicResolution: Boolean = false,
    /** A world four times larger, a longer view and 300 wandering creatures. */
    val heavy: Boolean = false,
    /** Above 0: a benchmark of this many frames with a fixed camera path. */
    val benchmarkFrames: Int = 0,
    /** More environment for the game (Mesa and Box64 options under test). */
    val extraEnv: Map<String, String> = emptyMap(),
)

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
