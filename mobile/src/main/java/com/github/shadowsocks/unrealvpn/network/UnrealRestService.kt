package com.github.shadowsocks.unrealvpn.network

import com.github.shadowsocks.unrealvpn.BASE_URL
import org.json.JSONObject

class UnrealRestService {

    private val httpClient = HttpClient()

    suspend fun getKey(): AccessKeyResponse {
        val rawResponse = httpClient.post("https://osa.unrealvpn.com/api/access-keys")
        val json = JSONObject(rawResponse)
        return AccessKeyResponse(
            keyId = json["id"].toString(),
            accessUrl = json["accessUrl"].toString(),
        )
    }

    suspend fun setLimits(keyId: String) {
        val limits = JSONObject()
            .put("bytes", 100000000)
        val requestBody = JSONObject()
            .put("limit", limits)

        httpClient.put(
            "$BASE_URL/access-keys/$keyId/data-limit",
            requestBody.toString()
        )
    }

    suspend fun registerKey(keyName: String, keyId: String) {
        val requestBody = JSONObject()
            .put("name", keyName)

        httpClient.put(
            "$BASE_URL/access-keys/$keyId/name",
            requestBody.toString()
        )
    }
}
