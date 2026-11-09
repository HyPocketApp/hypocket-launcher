package io.github.hypocketapp.blocks.ui

import android.annotation.SuppressLint
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.PointerId
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import io.github.hypocketapp.blocks.BlocksBenchmark
import io.github.hypocketapp.blocks.BlocksConfig
import io.github.hypocketapp.blocks.BlocksOptions
import io.github.hypocketapp.blocks.BlocksRecipes
import io.github.hypocketapp.blocks.BlocksReport
import io.github.hypocketapp.blocks.BlocksSession
import io.github.hypocketapp.blocks.BlocksStats
import io.github.hypocketapp.blocks.BlocksWorld
import io.github.hypocketapp.blocks.BlocksWorlds
import io.github.hypocketapp.blocks.DayMode
import io.github.hypocketapp.engine.DirectDisplay
import io.github.hypocketapp.engine.DisplayStatus
import io.github.hypocketapp.blocks.R
import io.github.hypocketapp.engine.view.FrameSurfaceView
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import kotlin.math.hypot
import kotlin.math.roundToInt

/**
 * Full-screen, landscape screen for HyPocket Blocks, the original test game:
 * the frames come from the game through [DirectDisplay], and touches go back
 * to it through [BlocksSession].
 *
 * The game cannot follow the app into the background (its display goes away
 * with the surface), so leaving the screen ends the session; a saved world is
 * written on the way out.
 */
class BlocksActivity : ComponentActivity() {
    private lateinit var session: BlocksSession
    private lateinit var controller: ControllerInput
    private lateinit var mouse: MouseInput
    private lateinit var selection: BlockSelection
    private lateinit var inventory: Inventory
    private var benchmark: BlocksBenchmark? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        // Use the area beside the camera cutout too, so the frame fills the screen.
        window.attributes = window.attributes.apply {
            layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        session = BlocksSession(this)
        val config = BlocksConfig.from(intent)
        val sensitivity = config.lookSensitivityPercent / 100f
        val store = worlds(this)
        val world = intent.getStringExtra(EXTRA_WORLD)?.let { store.get(it) }
        // Quick games are creative, like before worlds could be survival ones.
        val creative = world?.creative ?: true
        selection = BlockSelection(
            initial = world?.hotbar.orEmpty(),
            creative = creative,
            send = { session.selectBlock(it) },
            onArranged = { slots -> world?.let { store.saveHotbar(it.id, slots) } },
        )
        inventory = Inventory(recipes = if (creative) 0 else BlocksRecipes.ALL.size) { open ->
            // Stop walking and free the pointer while the inventory is up.
            controller.reset()
            if (open) mouse.release()
        }
        controller = ControllerInput(
            session, sensitivity, selection, inventory,
            craft = { i -> BlocksRecipes.ALL.getOrNull(i)?.let { session.craft(it) } },
            onExit = { session.stop() },
        )
        mouse = MouseInput(session, sensitivity, selection, paused = { inventory.open.value })
        if (intent.getBooleanExtra(EXTRA_BENCHMARK, false)) {
            val run = BlocksBenchmark(this).also { benchmark = it }
            val (width, height) = landscapeScreen(this)
            setContent { BlocksTheme { BenchmarkScreen(run, width, height, config.versionLine, onClose = { finish() }) } }
            return
        }
        val heavy = intent.getBooleanExtra(EXTRA_HEAVY, false)
        val dayMode = DayMode.fromName(intent.getStringExtra(EXTRA_DAY_MODE))
        setContent {
            BlocksTheme {
                BlocksScreen(
                    session = session,
                    controller = controller,
                    mouse = mouse,
                    selection = selection,
                    inventory = inventory,
                    world = world,
                    lookSensitivity = sensitivity,
                    renderScalePercent = config.renderScalePercent,
                    fpsLimit = config.fpsLimit,
                    versionLine = config.versionLine,
                    options = BlocksOptions(
                        dynamicResolution = config.dynamicResolution,
                        heavy = heavy,
                        world = world,
                        creative = creative,
                        dayMode = dayMode,
                        creatures = if (heavy) 0 else NORMAL_CREATURES,
                    ),
                    onClose = { finish() },
                )
            }
        }
    }

    // Controllers and keyboards reach the activity before Compose, which would
    // otherwise use the d-pad to move focus. androidx marks its override of
    // this public Activity method as restricted, which lint flags wrongly.
    @SuppressLint("RestrictedApi")
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        // With the mouse captured, Esc gives the pointer back instead of exiting.
        if (event.keyCode == KeyEvent.KEYCODE_ESCAPE && mouse.captured.value) {
            if (event.action == KeyEvent.ACTION_UP) mouse.release()
            return true
        }
        return controller.onKey(event) || super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean =
        mouse.onUncaptured(event) || controller.onMotion(event) || super.dispatchGenericMotionEvent(event)

    // A mouse click before capture arrives as a touch: it captures the pointer instead.
    override fun dispatchTouchEvent(event: MotionEvent): Boolean =
        mouse.onUncaptured(event) || super.dispatchTouchEvent(event)

    override fun onPointerCaptureChanged(hasCapture: Boolean) {
        super.onPointerCaptureChanged(hasCapture)
        mouse.onCaptureChanged(hasCapture)
    }

    override fun onStop() {
        super.onStop()
        if (!isChangingConfigurations) {
            session.stop()
            benchmark?.cancel()
            finish()
        }
    }

    override fun onDestroy() {
        session.close()
        super.onDestroy()
    }

    companion object {
        private const val EXTRA_HEAVY = "heavy"
        private const val EXTRA_DAY_MODE = "day_mode"
        private const val EXTRA_WORLD = "world"
        private const val EXTRA_BENCHMARK = "benchmark"

        /** Creatures outside heavy mode: some life without the heavy load. */
        private const val NORMAL_CREATURES = 40

        /** Where the player's worlds are kept. */
        fun worlds(context: Context) = BlocksWorlds(File(context.filesDir, "blocks-worlds"))

        /** Plays [world], loading and saving it, or a fresh unsaved world without one. */
        fun start(context: Context, config: BlocksConfig, heavy: Boolean, world: BlocksWorld?, dayMode: DayMode = DayMode.CYCLE) {
            val intent = Intent(context, BlocksActivity::class.java).putExtra(EXTRA_HEAVY, heavy).putExtra(EXTRA_WORLD, world?.id)
                .putExtra(EXTRA_DAY_MODE, dayMode.name)
            context.startActivity(config.putInto(intent))
        }

        /** Runs the benchmark instead of the game. */
        fun startBenchmark(context: Context, config: BlocksConfig) {
            context.startActivity(config.putInto(Intent(context, BlocksActivity::class.java).putExtra(EXTRA_BENCHMARK, true)))
        }
    }
}

/** The full screen's size in pixels, landscape (width >= height). */
private fun landscapeScreen(context: Context): Pair<Int, Int> {
    val (w, h) = if (android.os.Build.VERSION.SDK_INT >= 30) {
        val bounds = context.getSystemService(WindowManager::class.java).maximumWindowMetrics.bounds
        bounds.width() to bounds.height()
    } else {
        context.resources.displayMetrics.let { it.widthPixels to it.heightPixels }
    }
    return maxOf(w, h) to minOf(w, h)
}

@Composable
private fun BlocksScreen(
    session: BlocksSession,
    controller: ControllerInput,
    mouse: MouseInput,
    selection: BlockSelection,
    inventory: Inventory,
    world: BlocksWorld?,
    lookSensitivity: Float,
    renderScalePercent: Int,
    fpsLimit: Int,
    versionLine: String,
    options: BlocksOptions,
    onClose: () -> Unit,
) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val directDisplay = remember { CompletableDeferred<DirectDisplay?>() }
    var report by remember { mutableStateOf<BlocksReport?>(null) }
    var stats by remember { mutableStateOf(BlocksStats()) }

    LaunchedEffect(Unit) {
        val display = directDisplay.await()
        val (screenW, screenH) = landscapeScreen(context)
        // Even sizes keep the scaled frame free of half-pixel seams.
        val width = (screenW * renderScalePercent / 100) and 1.inv()
        val height = (screenH * renderScalePercent / 100) and 1.inv()
        report = withContext(Dispatchers.IO) { session.run(width, height, fpsLimit, display, options) }
    }
    LaunchedEffect(Unit) {
        while (report == null) {
            stats = session.stats
            delay(250)
        }
    }
    // Survival: blocks just collected get a place in the hotbar.
    LaunchedEffect(stats.counts) { if (stats.ready) selection.fill(stats.counts) }

    Box(Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView(
            factory = {
                FrameSurfaceView(it, session.frameFile, direct = true) { display -> directDisplay.complete(display) }
                    .also { view -> mouse.attach(view) }
            },
            modifier = Modifier.fillMaxSize(),
        )
        val finished = report
        if (finished == null && inventory.open.value) {
            InventoryPanel(inventory, selection, stats.counts, options.creative, onCraft = { session.craft(it) })
        } else if (finished == null) {
            TouchControls(session, lookSensitivity)
            Hud(stats, session, controller.used.value, mouse.captured.value, onExit = { session.stop() })
            Hotbar(
                selection,
                stats.counts,
                options.creative,
                onInventory = { inventory.show() },
                modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 12.dp),
            )
            Column(
                Modifier.align(Alignment.TopCenter).padding(top = 16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                if (stats.ready && stats.day > 0) Clock(stats)
                if (world != null) SavedNotice(stats.saves, Modifier)
            }
        }
        if (finished != null) {
            // The pointer works normally again on the report.
            LaunchedEffect(Unit) { mouse.stop() }
            ReportPanel(
                finished,
                versionLine,
                listOfNotNull(
                    world?.let { "World: ${it.chunks}x${it.chunks} chunks, saved ${finished.stats.saves} times" },
                    "Mode: ${if (options.creative) "creative" else "survival"}, carrying " +
                        BuildBlock.entries.mapIndexedNotNull { i, block ->
                            finished.stats.counts.getOrElse(i) { 0 }.takeIf { it > 0 }?.let { "${block.name.lowercase()} $it" }
                        }.ifEmpty { listOf("nothing") }.joinToString(),
                    "Input devices used: " +
                        (controller.deviceNames + mouse.deviceNames).ifEmpty { setOf("touch only") }.joinToString(),
                    "Input devices connected: " +
                        ControllerInput.connectedDevices().ifEmpty { listOf("none") }.joinToString(),
                    "Input events: ${controller.motionEvents} controller motion, ${controller.keyEvents} " +
                        "controller/keyboard keys, ${mouse.events} captured mouse",
                ),
                onClose,
            )
        }
    }
}

/** Left half: a joystick that appears where the thumb lands. Right half: drag to look, tap to break. */
@Composable
private fun TouchControls(session: BlocksSession, sensitivity: Float) {
    val density = LocalDensity.current
    val radiusPx = with(density) { 56.dp.toPx() }
    val lookPerPx = 0.0045f / density.density * 1.6f * sensitivity
    val tapSlopPx = with(density) { 10.dp.toPx() }
    var stickCenter by remember { mutableStateOf<Offset?>(null) }
    var stickKnob by remember { mutableStateOf(Offset.Zero) }

    Box(
        Modifier.fillMaxSize().pointerInput(Unit) {
            var stickPointer: PointerId? = null
            val lookStart = mutableMapOf<PointerId, Offset>()
            val lookMoved = mutableMapOf<PointerId, Boolean>()
            awaitPointerEventScope {
                while (true) {
                    val event = awaitPointerEvent()
                    for (change in event.changes) {
                        val id = change.id
                        when {
                            change.pressed && !change.previousPressed -> {
                                if (change.position.x < size.width / 2 && stickPointer == null) {
                                    stickPointer = id
                                    stickCenter = change.position
                                    stickKnob = Offset.Zero
                                } else {
                                    lookStart[id] = change.position
                                    lookMoved[id] = false
                                }
                            }
                            change.pressed -> {
                                if (id == stickPointer) {
                                    val center = stickCenter ?: change.position
                                    var d = change.position - center
                                    val length = d.getDistance()
                                    if (length > radiusPx) d *= radiusPx / length
                                    stickKnob = d
                                    session.setMove(d.x / radiusPx, -d.y / radiusPx)
                                } else if (id in lookStart) {
                                    val delta = change.position - change.previousPosition
                                    session.look(delta.x * lookPerPx, -delta.y * lookPerPx)
                                    val start = lookStart.getValue(id)
                                    if (hypot(change.position.x - start.x, change.position.y - start.y) > tapSlopPx) {
                                        lookMoved[id] = true
                                    }
                                }
                            }
                            !change.pressed && change.previousPressed -> {
                                if (id == stickPointer) {
                                    stickPointer = null
                                    stickCenter = null
                                    stickKnob = Offset.Zero
                                    session.setMove(0f, 0f)
                                } else if (lookStart.remove(id) != null) {
                                    if (lookMoved.remove(id) == false) session.breakBlock()
                                }
                            }
                        }
                        change.consume()
                    }
                }
            }
        },
    ) {
        stickCenter?.let { center ->
            val stickSize = with(density) { (radiusPx * 2).toDp() }
            val knobSize = 44.dp
            Box(
                Modifier
                    .offset { IntOffset((center.x - radiusPx).roundToInt(), (center.y - radiusPx).roundToInt()) }
                    .size(stickSize)
                    .background(Color.White.copy(alpha = 0.15f), CircleShape),
            )
            val knobHalf = with(density) { knobSize.toPx() / 2 }
            Box(
                Modifier
                    .offset {
                        IntOffset(
                            (center.x + stickKnob.x - knobHalf).roundToInt(),
                            (center.y + stickKnob.y - knobHalf).roundToInt(),
                        )
                    }
                    .size(knobSize)
                    .background(Color.White.copy(alpha = 0.35f), CircleShape),
            )
        }
    }
}

@Composable
private fun Hud(
    stats: BlocksStats,
    session: BlocksSession,
    controllerUsed: Boolean,
    mouseCaptured: Boolean,
    onExit: () -> Unit,
) {
    Box(Modifier.fillMaxSize().padding(16.dp)) {
        Column(Modifier.align(Alignment.TopStart), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            if (!stats.ready) {
                Text(stringResource(R.string.blocks_loading), color = Color.White)
            } else {
                Text(
                    stringResource(R.string.blocks_stats, stats.fps, stats.frameMillis, stats.triangles),
                    color = Color.White,
                    style = MaterialTheme.typography.bodySmall,
                )
                Text(
                    stringResource(R.string.blocks_resolution, stats.width, stats.height, stats.scalePercent),
                    color = Color.White,
                    style = MaterialTheme.typography.bodySmall,
                )
                if (stats.creatures > 0) {
                    Text(
                        stringResource(R.string.blocks_creatures, stats.creatures),
                        color = Color.White,
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
            val hint = when {
                mouseCaptured -> R.string.blocks_hint_mouse
                controllerUsed -> R.string.blocks_hint_controller
                else -> R.string.blocks_hint
            }
            Text(stringResource(hint), color = Color.White.copy(alpha = 0.7f), style = MaterialTheme.typography.bodySmall)
        }
        OutlinedButton(onClick = onExit, modifier = Modifier.align(Alignment.TopEnd)) {
            Text(stringResource(R.string.blocks_exit), color = Color.White)
        }
        Row(Modifier.align(Alignment.BottomEnd), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            // These sit above the touch layer, so pressing them does not turn the camera.
            HoldButton(stringResource(R.string.blocks_place), onPress = { session.placeBlock() }, onRelease = {})
            HoldButton(
                stringResource(R.string.blocks_jump),
                onPress = { session.setJump(true) },
                onRelease = { session.setJump(false) },
            )
        }
    }
}

/** The hotbar, the chosen block's name and count, and the inventory button. */
@Composable
private fun Hotbar(
    selection: BlockSelection,
    counts: List<Int>,
    creative: Boolean,
    onInventory: () -> Unit,
    modifier: Modifier,
) {
    val block = selection.block
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        val name = stringResource(block.label)
        Text(
            if (creative) name else stringResource(R.string.blocks_block_count, name, counts.getOrElse(block.ordinal) { 0 }),
            color = Color.White,
            style = MaterialTheme.typography.labelMedium,
            modifier = Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp))
                .padding(horizontal = 6.dp, vertical = 2.dp),
        )
        Row(Modifier.padding(top = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            HotbarRow(selection, counts, creative)
            // Above the touch layer, so pressing it does not turn the camera.
            Box(
                Modifier.padding(start = 6.dp).size(width = 44.dp, height = 36.dp)
                    .background(Color.Black.copy(alpha = 0.45f), RoundedCornerShape(6.dp))
                    .clickable(onClick = onInventory),
                contentAlignment = Alignment.Center,
            ) {
                Text(stringResource(R.string.blocks_inventory_short), color = Color.White, style = MaterialTheme.typography.labelMedium)
            }
        }
    }
}

/** The game's day and time, like "Day 2 · 18:40". */
@Composable
private fun Clock(stats: BlocksStats) {
    Text(
        stringResource(R.string.blocks_clock, stats.day, stats.clockMinutes / 60, stats.clockMinutes % 60),
        color = Color.White,
        style = MaterialTheme.typography.labelLarge,
        modifier = Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp))
            .padding(horizontal = 8.dp, vertical = 3.dp),
    )
}

/** A short "world saved" notice each time the game writes the save file. */
@Composable
private fun SavedNotice(saves: Int, modifier: Modifier) {
    var shownFor by remember { mutableIntStateOf(0) }
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(saves) {
        if (saves > shownFor) {
            shownFor = saves
            visible = true
            delay(2000)
            visible = false
        }
    }
    if (visible) {
        Text(
            stringResource(R.string.blocks_saved),
            color = Color.White,
            style = MaterialTheme.typography.labelMedium,
            modifier = modifier.background(Color.Black.copy(alpha = 0.45f), RoundedCornerShape(4.dp))
                .padding(horizontal = 8.dp, vertical = 3.dp),
        )
    }
}

@Composable
private fun HoldButton(label: String, onPress: () -> Unit, onRelease: () -> Unit) {
    Box(
        Modifier
            .size(72.dp)
            .background(Color.White.copy(alpha = 0.25f), CircleShape)
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown().consume()
                    onPress()
                    waitForUpOrCancellation()?.consume()
                    onRelease()
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = Color.White, style = MaterialTheme.typography.labelLarge)
    }
}

@Composable
private fun ReportPanel(report: BlocksReport, versionLine: String, inputLines: List<String>, onClose: () -> Unit) {
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    Column(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.85f)).padding(24.dp)
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(stringResource(R.string.blocks_report_title), color = Color.White, style = MaterialTheme.typography.titleMedium)
        Text(stringResource(report.status.label()), color = Color.White)
        val averageFps = if (report.seconds > 0) report.stats.frames / report.seconds else 0.0
        Text(stringResource(R.string.blocks_summary, report.stats.frames, averageFps), color = Color.White)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                scope.launch {
                    clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", report.toShareText(versionLine, inputLines))))
                }
            }) { Text(stringResource(R.string.blocks_copy_report)) }
            OutlinedButton(onClick = onClose) { Text(stringResource(R.string.blocks_close), color = Color.White) }
        }
    }
}

private fun DisplayStatus.label(): Int = when (this) {
    DisplayStatus.PASSED -> R.string.blocks_status_ended
    DisplayStatus.FAILED -> R.string.blocks_status_failed
    DisplayStatus.TIMED_OUT -> R.string.blocks_status_timed_out
    DisplayStatus.BLOCKED_SYSCALL -> R.string.blocks_status_blocked_syscall
    DisplayStatus.SERVER_FAILED -> R.string.blocks_status_server_failed
    DisplayStatus.NOT_AVAILABLE -> R.string.blocks_status_not_available
}

/** Plain-text report, in English, to paste into an issue. */
private fun BlocksReport.toShareText(versionLine: String, inputLines: List<String>): String = buildString {
    appendLine("HyPocket Blocks session")
    appendLine("Device: ${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
    appendLine(versionLine)
    appendLine("Android: ${android.os.Build.VERSION.RELEASE} (API ${android.os.Build.VERSION.SDK_INT})")
    appendLine("SoC: ${if (android.os.Build.VERSION.SDK_INT >= 31) android.os.Build.SOC_MODEL else "unknown"}")
    appendLine("Status: $status")
    appendLine("Direct display: $direct")
    appendLine("Renderer: ${renderer ?: "-"}")
    appendLine("Resolution: ${stats.width}x${stats.height}, 3D at ${stats.scalePercent}% at the end")
    appendLine("World built in: %.0f ms".format(stats.buildMillis))
    appendLine("Heavy mode: ${options.heavy}, dynamic resolution: ${options.dynamicResolution}, creative: ${options.creative}, time: ${options.dayMode}")
    appendLine("Frames: ${stats.frames} in %.1f s".format(seconds))
    appendLine("Time in the game at the end: day ${stats.day}, %02d:%02d".format(stats.clockMinutes / 60, stats.clockMinutes % 60))
    appendLine("Exit code: ${exitCode ?: "-"}")
    memory?.toLines()?.forEach { appendLine("Memory: $it") }
    appendLine("Audio: $audioInfo")
    inputLines.forEach { appendLine(it) }
    appendLine("--- game output ---")
    appendLine(output)
    if (directInfo.isNotBlank()) {
        appendLine("--- direct display ---")
        appendLine(directInfo)
    }
    appendLine("--- server output ---")
    append(serverOutput)
}
