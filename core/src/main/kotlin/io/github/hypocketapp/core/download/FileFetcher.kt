package io.github.hypocketapp.core.download

import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

/** Opens a download, optionally from [offset] to resume it. */
fun interface FileFetcher {
    /** Throws [IOException] on network errors and [ServerException] on HTTP errors. */
    fun open(url: String, offset: Long): FetchResponse
}

class FetchResponse(
    /** True when the server honoured [offset] (HTTP 206); otherwise the data starts at 0. */
    val resumed: Boolean,
    val stream: InputStream,
)

class ServerException(val status: Int) : IOException("HTTP $status")

/** [FileFetcher] built on [HttpURLConnection]. Only HTTPS is allowed. */
class UrlConnectionFetcher(private val timeoutMillis: Int = 20_000) : FileFetcher {
    override fun open(url: String, offset: Long): FetchResponse {
        require(url.startsWith("https://")) { "Only HTTPS downloads are allowed" }
        val connection = URL(url).openConnection() as HttpURLConnection
        connection.connectTimeout = timeoutMillis
        connection.readTimeout = timeoutMillis
        if (offset > 0) connection.setRequestProperty("Range", "bytes=$offset-")
        val status = connection.responseCode
        if (status !in 200..299) {
            connection.disconnect()
            throw ServerException(status)
        }
        return FetchResponse(resumed = status == HttpURLConnection.HTTP_PARTIAL, stream = connection.inputStream)
    }
}
