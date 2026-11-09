package io.github.hypocketapp.engine

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class RunLogTest {
    @get:Rule val folder = TemporaryFolder()

    private fun file(): File = File(folder.root, "20261109-083000-blocks.txt")

    @Test
    fun `a run cut off keeps its header and lines`() {
        val log = RunLog(file(), "Device: test\nApp: 1")
        log.line("blocks: first frame shown")
        log.line("[server] hypocket-stats: 5.0 s")
        // No finish: the process died here. Lines are on disk already.
        val entry = RunLogs.Entry(file())
        assertFalse(entry.finished)
        assertEquals("blocks", entry.kind)
        val text = entry.read()
        assertTrue(text.startsWith(RunLog.INTERRUPTED_MARKER))
        assertTrue(text.contains("Device: test"))
        assertTrue(text.contains("[server] hypocket-stats: 5.0 s"))
        log.close()
    }

    @Test
    fun `finish replaces the live lines with the report`() {
        val log = RunLog(file(), "Device: test")
        log.line("live line")
        log.finish("HyPocket Blocks session\nStatus: PASSED\n")
        log.line("after the end")
        val entry = RunLogs.Entry(file())
        assertTrue(entry.finished)
        assertEquals("HyPocket Blocks session\nStatus: PASSED\n", entry.read())
    }
}
