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
import androidx.activity.OnBackPressedCallback
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
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
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
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.PointerId
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.pluralStringResource
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
import io.github.hypocketapp.blocks.BlocksSettings
import io.github.hypocketapp.blocks.BlocksReport
import io.github.hypocketapp.blocks.BlocksSession
import io.github.hypocketapp.blocks.BlocksStats
import io.github.hypocketapp.blocks.BlocksWorld
import io.github.hypocketapp.blocks.BlocksWorlds
import io.github.hypocketapp.blocks.DayMode
import io.github.hypocketapp.blocks.NativeGame
import io.github.hypocketapp.engine.DirectDisplay
import io.github.hypocketapp.engine.DisplayStatus
import io.github.hypocketapp.engine.RunLogs
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
    private lateinit var pause: PauseMenu
    private lateinit var settingsStore: BlocksSettings
    private val settings = mutableStateOf<BlocksSettings.Values?>(null)
    private var benchmark: BlocksBenchmark? = null

    /** The game runs natively in this process, which then must end with it (see NativeGame). */
    private var native = false

    /** The pointer was captured when a screen opened over the game: capture it again on closing. */
    private var recapture = false

    /** The menu key (Esc or Start) pressed while the menu is open; it acts when released. */
    private var menuKey = 0

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
        settingsStore = BlocksSettings.of(this, config)
        settings.value = settingsStore.load()
        val sensitivity = { (settings.value?.sensitivityPercent ?: config.lookSensitivityPercent) / 100f }
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
            pointerFor(open)
        }
        pause = PauseMenu { open ->
            // The world stands still and the pointer is free while the menu is up.
            session.setPaused(open)
            controller.reset()
            pointerFor(open)
        }
        controller = ControllerInput(
            session, sensitivity, selection, inventory,
            craft = { i -> BlocksRecipes.ALL.getOrNull(i)?.let { session.craft(it) } },
            onExit = { pause.show() },
        )
        mouse = MouseInput(session, sensitivity, selection, paused = { inventory.open.value || pause.open.value })
        settings.value?.let { session.applySettings(it) }
        // Back: closes the inventory, steps back in the menu, or opens it.
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                when {
                    inventory.open.value -> inventory.close()
                    pause.open.value -> pause.back()
                    else -> pause.show()
                }
            }
        })
        if (intent.getBooleanExtra(EXTRA_BENCHMARK, false)) {
            val run = BlocksBenchmark(this).also { benchmark = it }
            val (width, height) = landscapeScreen(this)
            setContent { BlocksTheme { BenchmarkScreen(run, width, height, config.versionLine, onClose = { finish() }) } }
            return
        }
        val heavy = intent.getBooleanExtra(EXTRA_HEAVY, false)
        val dayMode = DayMode.fromName(intent.getStringExtra(EXTRA_DAY_MODE))
        // Native unless the tester asked for the emulated program (it tests the engine).
        native = NativeGame.isAvailable && !intent.getBooleanExtra(EXTRA_EMULATED, false)
        setContent {
            BlocksTheme {
                BlocksScreen(
                    session = session,
                    controller = controller,
                    mouse = mouse,
                    selection = selection,
                    inventory = inventory,
                    pause = pause,
                    settings = settings.value ?: settingsStore.load(),
                    onSettings = { changed -> changeSettings(changed) },
                    world = world,
                    lookSensitivity = sensitivity(),
                    renderScalePercent = config.renderScalePercent,
                    fpsLimit = config.fpsLimit,
                    versionLine = config.versionLine,
                    options = BlocksOptions(
                        dynamicResolution = config.dynamicResolution,
                        heavy = heavy,
                        world = world,
                        creative = creative,
                        peaceful = world?.peaceful ?: false,
                        dayMode = dayMode,
                        creatures = if (heavy) 0 else NORMAL_CREATURES,
                        native = native,
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
        // The pause menu is an ordinary screen: Android and Compose take the
        // keys (a controller's A falls back to the d-pad centre, B to Back).
        if (::pause.isInitialized && pause.open.value) {
            val code = event.keyCode
            if (code == KeyEvent.KEYCODE_BUTTON_START || code == KeyEvent.KEYCODE_ESCAPE) {
                // Act on release of a press made with the menu open: the release of
                // the press that opened it, and held-key repeats, change nothing.
                if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                    menuKey = code
                } else if (event.action == KeyEvent.ACTION_UP && menuKey == code) {
                    menuKey = 0
                    if (code == KeyEvent.KEYCODE_BUTTON_START) pause.resume() else pause.back()
                }
                return true
            }
            return super.dispatchKeyEvent(event)
        }
        menuKey = 0
        // Esc opens the menu straight away, captured pointer or not (the menu frees it).
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

    /** Frees the pointer while a screen is over the game, and captures it again after. */
    private fun pointerFor(open: Boolean) {
        if (open) {
            recapture = mouse.release() || recapture
        } else if (recapture && !inventory.open.value && !pause.open.value) {
            recapture = false
            mouse.capture()
        }
    }

    private fun changeSettings(changed: BlocksSettings.Values) {
        val before = settings.value ?: return
        settings.value = changed
        settingsStore.save(before, changed)
        session.applySettings(changed)
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
        // The native game's globals cannot be reset: the next game needs a fresh process.
        if (native && !isChangingConfigurations) android.os.Process.killProcess(android.os.Process.myPid())
    }

    companion object {
        private const val EXTRA_HEAVY = "heavy"
        private const val EXTRA_DAY_MODE = "day_mode"
        private const val EXTRA_WORLD = "world"
        private const val EXTRA_BENCHMARK = "benchmark"
        private const val EXTRA_EMULATED = "emulated"

        /** Creatures outside heavy mode: some life without the heavy load. */
        private const val NORMAL_CREATURES = 40

        /** Where the player's worlds are kept. */
        fun worlds(context: Context) = BlocksWorlds(File(context.filesDir, "blocks-worlds"))

        /** Plays [world], loading and saving it, or a fresh unsaved world without one. */
        fun start(
            context: Context,
            config: BlocksConfig,
            heavy: Boolean,
            world: BlocksWorld?,
            dayMode: DayMode = DayMode.CYCLE,
            emulated: Boolean = false,
        ) {
            val intent = Intent(context, BlocksActivity::class.java).putExtra(EXTRA_HEAVY, heavy).putExtra(EXTRA_WORLD, world?.id)
                .putExtra(EXTRA_DAY_MODE, dayMode.name).putExtra(EXTRA_EMULATED, emulated)
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
    pause: PauseMenu,
    settings: BlocksSettings.Values,
    onSettings: (BlocksSettings.Values) -> Unit,
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
    fun inputLines(finished: BlocksReport) = listOfNotNull(
        world?.let { "World: ${it.chunks}x${it.chunks} chunks, saved ${finished.stats.saves} times" },
        "Health: ${finished.stats.healthTenths / 10.0}, fainted ${finished.stats.deaths} times, " +
            "hurt ${finished.stats.hurts} times, peaceful: ${options.peaceful}",
        "Food: ${finished.stats.foodTenths / 10.0}, warmth: ${finished.stats.warmthTenths / 10.0}, " +
            "ate ${finished.stats.eats} times, picked up or hunted ${finished.stats.picks} times",
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
    )

    LaunchedEffect(Unit) {
        val display = directDisplay.await()
        val (screenW, screenH) = landscapeScreen(context)
        // Even sizes keep the scaled frame free of half-pixel seams.
        val width = (screenW * renderScalePercent / 100) and 1.inv()
        val height = (screenH * renderScalePercent / 100) and 1.inv()
        val log = RunLogs(context).start(
            "blocks",
            "HyPocket Blocks session\n" + deviceLines(versionLine) +
                "Heavy mode: ${options.heavy}, dynamic resolution: ${options.dynamicResolution}\n" +
                (world?.let { "World: ${it.chunks}x${it.chunks} chunks\n" } ?: ""),
        )
        report = try {
            withContext(Dispatchers.IO) {
                session.run(width, height, fpsLimit, display, options, log)
                    .also { log.finish(it.toShareText(versionLine, inputLines(it))) }
            }
        } finally {
            log.close()
        }
    }
    LaunchedEffect(Unit) {
        while (report == null) {
            stats = session.stats
            delay(250)
        }
    }
    // A note picked up in the world opens at once.
    var notesSeen by remember { mutableStateOf<Int?>(null) }
    LaunchedEffect(stats.ready, stats.notesOpened) {
        if (!stats.ready) return@LaunchedEffect
        val seen = notesSeen
        if (seen != null && stats.notesOpened > seen) pause.showNote(stats.noteId)
        notesSeen = stats.notesOpened
    }
    // Survival: blocks just collected get a place in the hotbar.
    LaunchedEffect(stats.counts) { if (stats.ready) selection.fill(stats.counts) }

    Box(Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView(
            factory = {
                if (options.native) {
                    directDisplay.complete(null)
                    val (screenW, screenH) = landscapeScreen(it)
                    // The phone's scaler stretches the smaller frame to the screen for free.
                    nativeSurface(it, (screenW * renderScalePercent / 100) and 1.inv(), (screenH * renderScalePercent / 100) and 1.inv())
                        .also { view -> mouse.attach(view) }
                } else {
                    FrameSurfaceView(it, session.frameFile, direct = true) { display -> directDisplay.complete(display) }
                        .also { view -> mouse.attach(view) }
                }
            },
            modifier = Modifier.fillMaxSize(),
        )
        val finished = report
        if (finished == null && inventory.open.value) {
            InventoryPanel(
                inventory, selection, stats.counts, options.creative, stats.survivalFlags, onCraft = { session.craft(it) },
            )
        } else if (finished == null) {
            if (!pause.open.value) TouchControls(session, lookSensitivity)
            Hud(stats, session, controller.used.value, mouse, settings.showStats, onMenu = { pause.show() })
            Hotbar(
                selection,
                stats.counts,
                options.creative,
                onInventory = { inventory.show() },
                mouse = mouse,
                modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 12.dp),
            )
            Column(
                Modifier.align(Alignment.TopCenter).padding(top = 16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                if (stats.ready && stats.away) {
                    Text(
                        stringResource(R.string.blocks_away),
                        color = Color(0xFFD9C8FF),
                        style = MaterialTheme.typography.labelLarge,
                        modifier = Modifier.background(Color.Black.copy(alpha = 0.45f), RoundedCornerShape(4.dp))
                            .padding(horizontal = 8.dp, vertical = 3.dp),
                    )
                } else if (stats.ready && stats.day > 0) {
                    Clock(stats)
                }
                if (stats.ready && stats.beacons > 0) BeaconCompass(stats)
                if (stats.sheltered) {
                    Text(
                        stringResource(R.string.blocks_sheltered),
                        color = Color(0xFFFFE3A8),
                        style = MaterialTheme.typography.labelLarge,
                        modifier = Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp))
                            .padding(horizontal = 8.dp, vertical = 3.dp),
                    )
                }
                if (world != null) SavedNotice(stats.saves, Modifier)
                EventNotice(stats.deaths, R.string.blocks_fainted)
                EventNotice(stats.takes, R.string.blocks_taken, Color(0xAA3A2466))
                ThingNotice(stats.picks, stats.picked, R.string.blocks_picked, Color(0xAA2E5A2E))
                ThingNotice(stats.needs, stats.needed, R.string.blocks_needs_tool, Color(0xAA5A4A1E))
                AwayNotice(stats.ready, stats.away)
            }
            if (stats.grabbed) GrabOverlay(stats.grabPercent)
            if (!options.creative) {
                Row(
                    Modifier.align(Alignment.BottomCenter).padding(bottom = 96.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    HealthBar(stats.healthTenths, Modifier)
                    Meter(
                        stats.foodTenths, BlocksStats.MAX_FOOD_TENTHS, R.string.blocks_meter_food,
                        Color(0xFFE0A040), Color(0xFF4A3418), Color.White,
                    )
                    Meter(
                        stats.warmthTenths, BlocksStats.MAX_WARMTH_TENTHS, R.string.blocks_meter_warmth,
                        if (stats.cold) Color(0xFF7FC4FF) else Color(0xFFFF9A5A), Color(0xFF1E2E4A),
                        when {
                            stats.warming -> Color(0xFFFFC08A)
                            stats.cold -> Color(0xFFB8E0FF)
                            else -> Color.White
                        },
                    )
                }
                HurtFlash(stats.hurts)
            }
            if (stats.digPercent in 1..99) {
                Box(
                    Modifier.align(Alignment.Center).padding(top = 40.dp).size(width = 44.dp, height = 4.dp)
                        .background(Color.Black.copy(alpha = 0.5f), RoundedCornerShape(2.dp)),
                ) {
                    Box(Modifier.fillMaxHeight().fillMaxWidth(stats.digPercent / 100f).background(Color.White, RoundedCornerShape(2.dp)))
                }
            }
            if (stats.aimingBeacon) {
                Text(
                    stringResource(if (options.creative) R.string.blocks_beacon_aim_free else R.string.blocks_beacon_aim),
                    color = Color.White,
                    style = MaterialTheme.typography.labelLarge,
                    modifier = Modifier.align(Alignment.Center).padding(top = 72.dp)
                        .background(Color.Black.copy(alpha = 0.5f), RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                )
            }
            GoalDone(stats)
            if ((stats.aimingNote || stats.aimingPickup) && !stats.aimingBeacon) {
                Text(
                    stringResource(if (stats.aimingNote) R.string.blocks_note_aim else R.string.blocks_pickup_aim),
                    color = Color.White,
                    style = MaterialTheme.typography.labelLarge,
                    modifier = Modifier.align(Alignment.Center).padding(top = 72.dp)
                        .background(Color.Black.copy(alpha = 0.5f), RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                )
            }
            if (pause.open.value) PausePanel(pause, settings, onSettings, onSaveAndExit = { session.stop() }, notesFound = stats.notesFound)
        }
        if (finished != null) {
            // The pointer works normally again on the report.
            LaunchedEffect(Unit) { mouse.stop() }
            ReportPanel(finished, versionLine, inputLines(finished), onClose)
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
        Modifier.fillMaxSize().pointerInput(lookPerPx) {
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
    mouse: MouseInput,
    showStats: Boolean,
    onMenu: () -> Unit,
) {
    val mouseCaptured = mouse.captured.value
    Box(Modifier.fillMaxSize().padding(16.dp)) {
        Column(Modifier.align(Alignment.TopStart), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            if (!stats.ready) {
                Text(stringResource(R.string.blocks_loading), color = Color.White)
            } else if (showStats) {
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
        OutlinedButton(onClick = onMenu, modifier = Modifier.align(Alignment.TopEnd).mouseClickArea(mouse, "menu")) {
            Text(stringResource(R.string.blocks_menu), color = Color.White)
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
    mouse: MouseInput,
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
                    .mouseClickArea(mouse, "inventory")
                    .clickable(onClick = onInventory),
                contentAlignment = Alignment.Center,
            ) {
                Text(stringResource(R.string.blocks_inventory_short), color = Color.White, style = MaterialTheme.typography.labelMedium)
            }
        }
    }
}

/** Where the nearest dark beacon is: an arrow turned towards it, how many are lit and how far. */
@Composable
private fun BeaconCompass(stats: BlocksStats) {
    Row(
        Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp)).padding(horizontal = 8.dp, vertical = 3.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        if (stats.beaconsLit < stats.beacons) {
            Text(
                stringResource(R.string.blocks_compass_arrow),
                color = Color(0xFFBFF4FF),
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.rotate(stats.beaconAngle.toFloat()),
            )
            Text(
                stringResource(R.string.blocks_beacons_far, stats.beaconsLit, stats.beacons, stats.beaconDistance),
                color = Color.White,
                style = MaterialTheme.typography.labelLarge,
            )
        } else {
            Text(pluralStringResource(R.plurals.blocks_beacons_all, stats.beacons, stats.beacons), color = Color(0xFFBFF4FF), style = MaterialTheme.typography.labelLarge)
        }
    }
}

/** Once, when the last beacon is lit: the world is finished, with the time it took. */
@Composable
private fun GoalDone(stats: BlocksStats) {
    var seenDone by remember { mutableStateOf<Boolean?>(null) }
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(stats.ready, stats.goalDone) {
        if (!stats.ready) return@LaunchedEffect
        // A world finished before this session opens without the banner.
        if (seenDone == null) seenDone = stats.goalDone else if (stats.goalDone && seenDone == false) {
            seenDone = true
            visible = true
        }
    }
    if (!visible) return
    Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.55f)), contentAlignment = Alignment.Center) {
        Column(
            Modifier.background(Color(0xEE14303A), RoundedCornerShape(10.dp)).padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(stringResource(R.string.blocks_goal_done_title), color = Color.White, style = MaterialTheme.typography.headlineSmall)
            Text(
                stringResource(R.string.blocks_goal_done_text, formatDuration(stats.doneSeconds)),
                color = Color.White,
                style = MaterialTheme.typography.bodyMedium,
            )
            Button(onClick = { visible = false }) { Text(stringResource(R.string.blocks_keep_playing)) }
        }
    }
}

/** "1 h 05 min" or "12 min 30 s". */
@Composable
fun formatDuration(seconds: Int): String =
    if (seconds >= 3600) {
        stringResource(R.string.blocks_duration_hours, seconds / 3600, seconds % 3600 / 60)
    } else {
        stringResource(R.string.blocks_duration_minutes, seconds / 60, seconds % 60)
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

/** Ten bars of health, red ones lost; it only shows when health is not full. */
@Composable
private fun HealthBar(tenths: Int, modifier: Modifier) {
    if (tenths >= BlocksStats.MAX_HEALTH_TENTHS) return
    Row(
        modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp)).padding(3.dp),
        horizontalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        repeat(BlocksStats.MAX_HEALTH_TENTHS / 10) { i ->
            val fill = ((tenths - i * 10) / 10f).coerceIn(0f, 1f)
            Box(Modifier.size(width = 14.dp, height = 8.dp).background(Color(0xFF5A1E1E), RoundedCornerShape(2.dp))) {
                Box(Modifier.fillMaxHeight().fillMaxWidth(fill).background(Color(0xFFE8574A), RoundedCornerShape(2.dp)))
            }
        }
    }
}

/**
 * A bar of [tenths] out of [max] with its name ([label], in [labelColor]); it
 * only shows when not full.
 */
@Composable
private fun Meter(tenths: Int, max: Int, label: Int, fill: Color, empty: Color, labelColor: Color) {
    if (tenths >= max) return
    Row(
        Modifier.background(Color.Black.copy(alpha = 0.35f), RoundedCornerShape(4.dp)).padding(horizontal = 4.dp, vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(stringResource(label), color = labelColor, style = MaterialTheme.typography.labelSmall)
        Box(Modifier.size(width = 70.dp, height = 8.dp).background(empty, RoundedCornerShape(2.dp))) {
            Box(
                Modifier.fillMaxHeight().fillMaxWidth((tenths / max.toFloat()).coerceIn(0f, 1f))
                    .background(fill, RoundedCornerShape(2.dp)),
            )
        }
    }
}

/** Says for a few seconds [text] about a block or item ([thing], a [BuildBlock] index), each time [count] goes up. */
@Composable
private fun ThingNotice(count: Int, thing: Int, text: Int, background: Color) {
    var seen by remember { mutableIntStateOf(count) }
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(count) {
        if (count > seen) {
            seen = count
            visible = true
            delay(2500)
            visible = false
        }
    }
    val block = BuildBlock.entries.getOrNull(thing) ?: return
    if (visible) {
        Text(
            stringResource(text, stringResource(block.label)),
            color = Color.White,
            style = MaterialTheme.typography.labelLarge,
            modifier = Modifier.background(background, RoundedCornerShape(4.dp)).padding(horizontal = 10.dp, vertical = 3.dp),
        )
    }
}

/** A short red flash over the screen each time the player is hurt. */
@Composable
private fun HurtFlash(hurts: Int) {
    var seen by remember { mutableIntStateOf(hurts) }
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(hurts) {
        if (hurts > seen) {
            seen = hurts
            visible = true
            delay(250)
            visible = false
        }
    }
    if (visible) Box(Modifier.fillMaxSize().background(Color(0x55D02020)))
}

/** Says for a few seconds what happened, each time [count] goes up. */
@Composable
private fun EventNotice(count: Int, text: Int, background: Color = Color(0xAA6E1A1A)) {
    var seen by remember { mutableIntStateOf(count) }
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(count) {
        if (count > seen) {
            seen = count
            visible = true
            delay(5000)
            visible = false
        }
    }
    if (visible) {
        Text(
            stringResource(text),
            color = Color.White,
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.background(background, RoundedCornerShape(4.dp)).padding(horizontal = 10.dp, vertical = 4.dp),
        )
    }
}

/** Says where the player woke up when they reach the other side, and when they get back. */
@Composable
private fun AwayNotice(ready: Boolean, away: Boolean) {
    var was by remember { mutableStateOf<Boolean?>(null) }
    var text by remember { mutableIntStateOf(0) }
    LaunchedEffect(ready, away) {
        if (!ready) return@LaunchedEffect
        val before = was
        was = away
        if (before == null || before == away) return@LaunchedEffect
        text = if (away) R.string.blocks_away_arrived else R.string.blocks_away_back
        delay(7000)
        text = 0
    }
    if (text != 0) {
        Text(
            stringResource(text),
            color = Color.White,
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.background(Color(0xAA3A2466), RoundedCornerShape(4.dp)).padding(horizontal = 10.dp, vertical = 4.dp),
        )
    }
}

/**
 * While a night creature drags the player: the screen closes in, and a bar
 * shows how far they have got free (each press of jump helps).
 */
@Composable
private fun GrabOverlay(percent: Int) {
    Box(
        Modifier.fillMaxSize().background(
            androidx.compose.ui.graphics.Brush.radialGradient(
                0.35f to Color.Transparent,
                1f to Color(0xE0100818),
            ),
        ),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.padding(top = 140.dp).background(Color.Black.copy(alpha = 0.55f), RoundedCornerShape(6.dp))
                .padding(horizontal = 12.dp, vertical = 6.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text(stringResource(R.string.blocks_grabbed), color = Color.White, style = MaterialTheme.typography.titleSmall)
            Box(Modifier.size(width = 160.dp, height = 8.dp).background(Color(0xFF3A2466), RoundedCornerShape(3.dp))) {
                Box(
                    Modifier.fillMaxHeight().fillMaxWidth((percent / 100f).coerceIn(0f, 1f))
                        .background(Color(0xFFD9C8FF), RoundedCornerShape(3.dp)),
                )
            }
        }
    }
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
        val first = remember { FocusRequester() }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onClose, modifier = Modifier.focusRequester(first)) {
                Text(stringResource(R.string.blocks_back_to_worlds))
            }
            OutlinedButton(onClick = {
                scope.launch {
                    clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("HyPocket report", report.toShareText(versionLine, inputLines))))
                }
            }) { Text(stringResource(R.string.blocks_copy_report), color = Color.White) }
        }
        LaunchedEffect(Unit) { runCatching { first.requestFocus() } }
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
    append(deviceLines(versionLine))
    appendLine("Status: $status")
    appendLine("Direct display: $direct")
    appendLine("Renderer: ${renderer ?: "-"}")
    appendLine("Resolution: ${stats.width}x${stats.height}, 3D at ${stats.scalePercent}% at the end")
    appendLine("World built in: %.0f ms".format(stats.buildMillis))
    appendLine("Heavy mode: ${options.heavy}, dynamic resolution: ${options.dynamicResolution}, creative: ${options.creative}, time: ${options.dayMode}")
    appendLine("Frames: ${stats.frames} in %.1f s".format(seconds))
    appendLine("Beacons: ${stats.beaconsLit} of ${stats.beacons} lit, goal done: ${stats.goalDone}, played ${stats.playSeconds} s")
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

/** The native game's window: its surface goes to the game as it comes and goes. */
private fun nativeSurface(context: Context, width: Int, height: Int): android.view.SurfaceView =
    android.view.SurfaceView(context).apply {
        holder.setFixedSize(width, height)
        holder.addCallback(object : android.view.SurfaceHolder.Callback {
            override fun surfaceCreated(holder: android.view.SurfaceHolder) = NativeGame.setSurface(holder.surface)

            override fun surfaceChanged(holder: android.view.SurfaceHolder, format: Int, width: Int, height: Int) =
                NativeGame.setSurface(holder.surface)

            override fun surfaceDestroyed(holder: android.view.SurfaceHolder) = NativeGame.setSurface(null)
        })
    }
