package io.github.hypocketapp.core.download

import io.github.hypocketapp.core.account.VerifiedAccount
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream
import java.security.MessageDigest

class ManifestGameDownloaderTest {
    @get:Rule
    val folder = TemporaryFolder()

    private val account = VerifiedAccount("uuid-1", "Player", 0L)
    private val data = ByteArray(300_000) { (it * 31 % 251).toByte() }
    private val file = ManifestFile("bin/client.dat", "https://example.invalid/client.dat", data.size.toLong(), sha256(data))

    private fun sha256(bytes: ByteArray) =
        MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }

    /** Serves [content], honouring ranges unless [supportsRanges] is false. */
    private class FakeFetcher(
        private val content: ByteArray,
        private val supportsRanges: Boolean = true,
        private val failAfterBytes: Int? = null,
    ) : FileFetcher {
        val offsets = mutableListOf<Long>()

        override fun open(url: String, offset: Long): FetchResponse {
            offsets += offset
            val start = if (supportsRanges) offset.toInt() else 0
            var stream: InputStream = ByteArrayInputStream(content, start, content.size - start)
            failAfterBytes?.let { limit -> stream = FailingStream(stream, limit) }
            return FetchResponse(resumed = supportsRanges && offset > 0, stream = stream)
        }
    }

    private class FailingStream(private val inner: InputStream, private var left: Int) : InputStream() {
        override fun read(): Int = throw UnsupportedOperationException()
        override fun read(b: ByteArray, off: Int, len: Int): Int {
            if (left <= 0) throw IOException("connection lost")
            val n = inner.read(b, off, minOf(len, left))
            if (n > 0) left -= n
            return n
        }
    }

    private fun downloader(
        fetcher: FileFetcher,
        manifest: GameManifest? = GameManifest("1", listOf(file)),
        free: Long = Long.MAX_VALUE,
        wifiOnly: Boolean = false,
        onWifi: Boolean = true,
    ) = ManifestGameDownloader(
        source = { manifest },
        fetcher = fetcher,
        installDir = folder.root,
        freeBytes = { free },
        wifiOnly = { wifiOnly },
        isOnWifi = { onWifi },
    )

    @Test
    fun `downloads and checks a file`() = runTest {
        val states = downloader(FakeFetcher(data)).download(account).toList()

        assertEquals(DownloadState.Completed, states.last())
        assertArrayEquals(data, folder.root.resolve("bin/client.dat").readBytes())
        assertFalse(folder.root.resolve("bin/client.dat.part").exists())
    }

    @Test
    fun `resumes an interrupted download`() = runTest {
        val first = downloader(FakeFetcher(data, failAfterBytes = 100_000)).download(account).toList()
        assertEquals(DownloadState.Failed(DownloadState.FailureReason.NETWORK), first.last())

        val fetcher = FakeFetcher(data)
        val second = downloader(fetcher).download(account).toList()

        assertEquals(DownloadState.Completed, second.last())
        assertEquals(listOf(100_000L), fetcher.offsets)
        assertArrayEquals(data, folder.root.resolve("bin/client.dat").readBytes())
    }

    @Test
    fun `starts over when the server ignores the range`() = runTest {
        downloader(FakeFetcher(data, failAfterBytes = 100_000)).download(account).toList()

        val states = downloader(FakeFetcher(data, supportsRanges = false)).download(account).toList()

        assertEquals(DownloadState.Completed, states.last())
        assertArrayEquals(data, folder.root.resolve("bin/client.dat").readBytes())
    }

    @Test
    fun `damaged file is deleted and never installed`() = runTest {
        val damaged = data.copyOf().also { it[1234] = (it[1234] + 1).toByte() }

        val states = downloader(FakeFetcher(damaged)).download(account).toList()

        assertEquals(DownloadState.Failed(DownloadState.FailureReason.CORRUPTED), states.last())
        assertFalse(folder.root.resolve("bin/client.dat").exists())
        assertFalse(folder.root.resolve("bin/client.dat.part").exists())
    }

    @Test
    fun `files already on the phone are not downloaded again`() = runTest {
        downloader(FakeFetcher(data)).download(account).toList()
        val fetcher = FakeFetcher(data)

        val states = downloader(fetcher).download(account).toList()

        assertEquals(DownloadState.Completed, states.last())
        assertTrue(fetcher.offsets.isEmpty())
    }

    @Test
    fun `paths that leave the game folder are refused`() = runTest {
        val evil = file.copy(path = "../outside.dat")
        val fetcher = FakeFetcher(data)

        val states = downloader(fetcher, GameManifest("1", listOf(evil))).download(account).toList()

        assertEquals(DownloadState.Failed(DownloadState.FailureReason.CORRUPTED), states.last())
        assertTrue(fetcher.offsets.isEmpty())
        assertFalse(folder.root.parentFile!!.resolve("outside.dat").exists())
    }

    @Test
    fun `stops when there is not enough space`() = runTest {
        val states = downloader(FakeFetcher(data), free = 1_000).download(account).toList()

        assertTrue(states.last() is DownloadState.NotEnoughSpace)
    }

    @Test
    fun `waits for Wi-Fi when asked to`() = runTest {
        val fetcher = FakeFetcher(data)
        val states = downloader(fetcher, wifiOnly = true, onWifi = false).download(account).toList()

        assertEquals(DownloadState.WaitingForWifi, states.last())
        assertTrue(fetcher.offsets.isEmpty())
    }

    @Test
    fun `reports when there is no official source`() = runTest {
        val states = downloader(FakeFetcher(data), manifest = null).download(account).toList()

        assertTrue(states.last() is DownloadState.Unavailable)
    }
}
