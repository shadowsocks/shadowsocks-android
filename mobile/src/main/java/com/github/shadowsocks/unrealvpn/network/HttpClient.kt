package com.github.shadowsocks.unrealvpn.network

import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

class HttpClient {
    @Throws(IOException::class)
    fun post(url: String): String {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.setRequestMethod("POST")
        return conn.inputStream.bufferedReader().readText()
    }

    @Throws(IOException::class)
    fun put(url: String, body: String?): String {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.setRequestMethod("PUT")
        conn.setRequestProperty("Content-Type", "application/json")
        body?.let {
            conn.outputStream.use {
                it.write(body.toByteArray())
            }
        }
        return conn.inputStream.bufferedReader().readText()
    }
}
