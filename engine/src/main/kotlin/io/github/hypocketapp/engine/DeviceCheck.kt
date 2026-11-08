package io.github.hypocketapp.engine

import android.app.ActivityManager
import android.content.Context
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.GLES20
import android.opengl.GLES30
import android.os.Build

/**
 * Checks whether this phone can run the game through HyPocket's engine: an
 * ARM64 CPU, OpenGL ES 3.2 (the VirGL server needs it to offer the desktop
 * OpenGL 4.1 the game uses) and enough memory.
 *
 * Blocking (it creates a small OpenGL ES context): call it from a background
 * thread.
 */
object DeviceCheck {
    /** Below this the game is not expected to run at all. */
    private const val MIN_GLES = 31

    /** What we tested with (Mali-G52); 3.1 might work but is untested. */
    private const val RECOMMENDED_GLES = 32

    /**
     * A "4 GB" phone reports about 3.6 GB to apps (the Galaxy M12 reports
     * 3704 MB), so this flags 3 GB phones and smaller.
     */
    private const val RECOMMENDED_RAM_MB = 3400L

    fun run(context: Context): DeviceReport {
        val am = context.getSystemService(ActivityManager::class.java)
        val memory = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        val gles = queryGles()
        val ramMb = memory.totalMem / (1024 * 1024)

        val problems = buildList {
            if (Engine.TARGET_ABI !in Build.SUPPORTED_ABIS) add(DeviceProblem.NOT_ARM64)
            when {
                gles == null -> add(DeviceProblem.NO_GLES3)
                gles.version < MIN_GLES -> add(DeviceProblem.GLES_TOO_OLD)
                gles.version < RECOMMENDED_GLES -> add(DeviceProblem.GLES_UNTESTED)
            }
            if (ramMb < RECOMMENDED_RAM_MB) add(DeviceProblem.LOW_RAM)
        }

        return DeviceReport(
            glesVersion = gles?.let { "${it.version / 10}.${it.version % 10}" },
            glesRenderer = gles?.renderer,
            totalRamMb = ramMb,
            problems = problems,
        )
    }

    private class GlesInfo(val version: Int, val renderer: String?)

    /** Creates a 1x1 OpenGL ES 3 context to read the driver's version. */
    private fun queryGles(): GlesInfo? {
        val display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        if (display == EGL14.EGL_NO_DISPLAY) return null
        val version = IntArray(2)
        if (!EGL14.eglInitialize(display, version, 0, version, 1)) return null
        try {
            val configs = arrayOfNulls<EGLConfig>(1)
            val count = IntArray(1)
            val attribs = intArrayOf(
                EGL14.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_NONE,
            )
            if (!EGL14.eglChooseConfig(display, attribs, 0, configs, 0, 1, count, 0) || count[0] == 0) return null
            val config = configs[0]
            val context = EGL14.eglCreateContext(
                display, config, EGL14.EGL_NO_CONTEXT,
                intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 3, EGL14.EGL_NONE), 0,
            )
            if (context == EGL14.EGL_NO_CONTEXT) return null
            val surface = EGL14.eglCreatePbufferSurface(
                display, config, intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE), 0,
            )
            try {
                if (surface == EGL14.EGL_NO_SURFACE || !EGL14.eglMakeCurrent(display, surface, surface, context)) {
                    return null
                }
                val major = IntArray(1)
                val minor = IntArray(1)
                GLES30.glGetIntegerv(GLES30.GL_MAJOR_VERSION, major, 0)
                GLES30.glGetIntegerv(GLES30.GL_MINOR_VERSION, minor, 0)
                val renderer = GLES20.glGetString(GLES20.GL_RENDERER)
                EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
                return GlesInfo(major[0] * 10 + minor[0].coerceAtMost(9), renderer)
            } finally {
                if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, surface)
                EGL14.eglDestroyContext(display, context)
            }
        } finally {
            EGL14.eglTerminate(display)
        }
    }

    /** EGL_OPENGL_ES3_BIT_KHR, not in EGL14. */
    private const val EGL_OPENGL_ES3_BIT = 0x40
}

enum class DeviceProblem(val blocksGame: Boolean) {
    NOT_ARM64(true),
    NO_GLES3(true),
    GLES_TOO_OLD(true),
    GLES_UNTESTED(false),
    LOW_RAM(false),
}

data class DeviceReport(
    val glesVersion: String?,
    val glesRenderer: String?,
    val totalRamMb: Long,
    val problems: List<DeviceProblem>,
) {
    /** False when something rules the game out on this phone. */
    val canRun: Boolean get() = problems.none { it.blocksGame }
}
