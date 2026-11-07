package io.github.hypocketapp.core.auth

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

/** Minimal HTTP client, so the auth flow can be tested without a network. */
fun interface HttpTransport {
    /** Sends an `application/x-www-form-urlencoded` POST and returns the response. */
    suspend fun postForm(url: String, form: Map<String, String>): HttpResponse
}

data class HttpResponse(val status: Int, val body: String)

/** [HttpTransport] built on [HttpURLConnection], to avoid extra dependencies. */
class UrlConnectionTransport(
    private val timeoutMillis: Int = 15_000,
) : HttpTransport {
    override suspend fun postForm(url: String, form: Map<String, String>): HttpResponse =
        withContext(Dispatchers.IO) {
            val body = form.entries.joinToString("&") { (key, value) ->
                "${encode(key)}=${encode(value)}"
            }
            val connection = URL(url).openConnection() as HttpURLConnection
            try {
                connection.requestMethod = "POST"
                connection.connectTimeout = timeoutMillis
                connection.readTimeout = timeoutMillis
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
                connection.setRequestProperty("Accept", "application/json")
                connection.outputStream.use { it.write(body.toByteArray()) }

                val status = connection.responseCode
                val stream = if (status in 200..299) connection.inputStream else connection.errorStream
                val text = stream?.bufferedReader()?.use { it.readText() }.orEmpty()
                HttpResponse(status, text)
            } finally {
                connection.disconnect()
            }
        }

    private fun encode(value: String): String = URLEncoder.encode(value, "UTF-8")
}
