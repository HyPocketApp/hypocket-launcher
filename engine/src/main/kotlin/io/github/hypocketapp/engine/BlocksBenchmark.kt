package io.github.hypocketapp.engine

import android.content.Context

/**
 * Runs HyPocket Blocks in heavy mode several times with a fixed camera path,
 * each time with different Mesa or Box64 options or resolution, to find
 * which ones make draw calls and geometry cheaper on this phone.
 *
 * Blocking: call it from a background thread. All phases share one
 * [DirectDisplay], which accepts one VirGL server after another.
 */
class BlocksBenchmark(private val context: Context) {
    @Volatile private var cancelled = false
    @Volatile private var current: BlocksSession? = null

    /** Stops the running phase and skips the rest. */
    fun cancel() {
        cancelled = true
        current?.stop()
    }

    data class Phase(
        val name: String,
        /** Environment for the game. */
        val env: Map<String, String> = emptyMap(),
        /** Share of the screen's width and height. */
        val scalePercent: Int = 100,
    )

    data class PhaseResult(
        val phase: Phase,
        val status: DisplayStatus,
        val fps: Double?,
        /** The game's own work per frame (through Box64), and its wait for the GPU and server. */
        val cpuMillis: Double?,
        val waitMillis: Double?,
        /** Time the VirGL server spent running the game's GL commands per frame, and how busy it was. */
        val serverMillis: Double?,
        val serverBusyPercent: Double?,
        val report: BlocksReport,
    )

    /** [onPhase] gets the index of the phase about to run. Phases skipped by [cancel] are left out. */
    fun run(screenWidth: Int, screenHeight: Int, display: DirectDisplay?, onPhase: (Int) -> Unit): List<PhaseResult> =
        PHASES.withIndex().asSequence().takeWhile { !cancelled }.map { (index, phase) ->
            onPhase(index)
            val width = (screenWidth * phase.scalePercent / 100) and 1.inv()
            val height = (screenHeight * phase.scalePercent / 100) and 1.inv()
            val report = BlocksSession(context).use { session ->
                current = session
                if (cancelled) session.stop()
                session.run(
                    width, height, fpsLimit = 0, display = display,
                    options = BlocksOptions(heavy = true, benchmarkFrames = FRAMES, extraEnv = phase.env),
                )
            }
            val bench = BENCH_LINE.find(report.output)
            val server = SERVER_LINE.find(report.serverOutput)
            PhaseResult(
                phase = phase,
                status = report.status,
                fps = bench?.groupValues?.get(1)?.toDoubleOrNull(),
                cpuMillis = bench?.groupValues?.get(2)?.toDoubleOrNull(),
                waitMillis = bench?.groupValues?.get(3)?.toDoubleOrNull(),
                serverMillis = server?.groupValues?.get(1)?.toDoubleOrNull(),
                serverBusyPercent = server?.groupValues?.get(2)?.toDoubleOrNull(),
                report = report,
            )
        }.toList()

    companion object {
        /** Frames per phase, the first 60 left out of the averages. */
        const val FRAMES = 600

        val PHASES = listOf(
            Phase("baseline"),
            // Mesa records GL calls on the game's thread and runs them on its own.
            Phase("glthread", mapOf("mesa_glthread" to "true")),
            // Box64 assumes calls and returns never need flags, and adds more entry points.
            Phase(
                "glthread + Box64 tuned",
                mapOf("mesa_glthread" to "true", "BOX64_DYNAREC_SAFEFLAGS" to "0", "BOX64_DYNAREC_SEP" to "1"),
            ),
            // A quarter of the pixels: tells pixel work from geometry and draw calls.
            Phase("half resolution", scalePercent = 50),
        )

        private val BENCH_LINE =
            Regex("""bench: \d+ frames in \d+ ms: ([\d.]+) fps, [\d.]+ ms/frame, cpu ([\d.]+) ms, wait ([\d.]+) ms""")
        private val SERVER_LINE =
            Regex("""hypocket-stats: total [\d.]+ s, \d+ frames: commands ([\d.]+) ms/frame \(([\d.]+)% busy\)""")
    }
}
