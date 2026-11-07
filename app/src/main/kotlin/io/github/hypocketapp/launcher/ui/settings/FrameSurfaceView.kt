package io.github.hypocketapp.launcher.ui.settings

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.view.Choreographer
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.core.graphics.createBitmap
import androidx.core.graphics.withScale
import io.github.hypocketapp.engine.SharedFrame
import java.io.File

/**
 * Shows the frames the display probe writes into [frameFile], once per screen
 * refresh, and counts how many new frames it showed.
 */
// Created only from code, never inflated from XML.
@SuppressLint("ViewConstructor")
class FrameSurfaceView(context: Context, private val frameFile: File) :
    SurfaceView(context), SurfaceHolder.Callback, Choreographer.FrameCallback {

    private var frame: SharedFrame? = null
    private var bitmap: Bitmap? = null
    private var surfaceReady = false
    private val paint = Paint(Paint.FILTER_BITMAP_FLAG)

    /** New frames shown so far. */
    @Volatile
    var shownFrames = 0
        private set

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceReady = true
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) = Unit

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        surfaceReady = false
        Choreographer.getInstance().removeFrameCallback(this)
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!surfaceReady) return
        val source = frame ?: SharedFrame.open(frameFile)?.also { frame = it }
        if (source != null) {
            val target = bitmap ?: createBitmap(source.width, source.height, Bitmap.Config.ARGB_8888)
                .also { bitmap = it }
            if (source.copyNewFrame(target)) {
                shownFrames++
                draw(target)
            }
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    private fun draw(image: Bitmap) {
        val canvas = holder.lockHardwareCanvas() ?: return
        try {
            canvas.drawColor(Color.BLACK)
            // Fit the frame in the view, keeping its aspect ratio.
            val scale = minOf(width.toFloat() / image.width, height.toFloat() / image.height)
            val w = image.width * scale
            val h = image.height * scale
            val dst = RectF((width - w) / 2, (height - h) / 2, (width + w) / 2, (height + h) / 2)
            // glReadPixels returns the bottom row first, so flip vertically.
            canvas.withScale(1f, -1f, width / 2f, height / 2f) {
                drawBitmap(image, Rect(0, 0, image.width, image.height), dst, paint)
            }
        } finally {
            holder.unlockCanvasAndPost(canvas)
        }
    }
}
