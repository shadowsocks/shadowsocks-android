package com.github.shadowsocks.unrealvpn.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

class HttpClient {
    @Throws(IOException::class)
    suspend fun post(url: String): String {
        return withContext(Dispatchers.IO) {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.setRequestMethod("POST")
            conn.inputStream.bufferedReader().readText()
        }
    }

    @Throws(IOException::class)
    suspend fun put(url: String, body: String?): String {
        return withContext(Dispatchers.IO) {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.setRequestMethod("PUT")
            conn.setRequestProperty("Content-Type", "application/json")
            body?.let {
                conn.outputStream.use {
                    it.write(body.toByteArray())
                }
            }
            conn.inputStream.bufferedReader().readText()
        }
    }
}
