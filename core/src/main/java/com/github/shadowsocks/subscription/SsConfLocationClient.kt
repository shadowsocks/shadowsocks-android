/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2026                                                         *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.subscription

import com.github.shadowsocks.utils.SubscriptionUrls
import com.github.shadowsocks.utils.useCancellable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

object SsConfLocationClient {
    private const val PRODUCT_CODE_HEADER = "Product-Code"
    private const val PRODUCT_CODE_VALUE = "VANYA"
    private const val LOCATION_LIST_PATH = "/app/v1/sync/available-locations"
    private const val LOCATION_CHANGE_PATH = "/app/v1/user/location/change"

    data class Location(
            val value: String,
            val description: String,
            val code: String?,
            val speed: String?,
            val bestLocation: Boolean,
            val systemLocation: Boolean,
    )

    data class ChangeResult(val tag: String?)

    class ApiException(val statusCode: Int, message: String? = null) : IOException(message ?: "HTTP $statusCode")

    fun supports(subscriptionUrl: String?) = subscriptionUrl != null && SubscriptionUrls.parseDynamic(subscriptionUrl) != null

    suspend fun fetchLocations(subscriptionUrl: String, languageTag: String): List<Location> {
        val source = SubscriptionUrls.parseDynamic(subscriptionUrl)
                ?: throw IllegalArgumentException("Subscription URL does not support location changes")
        val body = request(source.endpoint(LOCATION_LIST_PATH, languageTag))
        return parseLocations(body)
    }

    suspend fun changeLocation(subscriptionUrl: String, location: String, languageTag: String): ChangeResult {
        val source = SubscriptionUrls.parseDynamic(subscriptionUrl)
                ?: throw IllegalArgumentException("Subscription URL does not support location changes")
        val body = request(source.endpoint(LOCATION_CHANGE_PATH, languageTag, location))
        return parseChangeResult(body)
    }

    internal fun parseLocations(body: String): List<Location> {
        val result = JSONArray(body)
        return List(result.length()) { index ->
            result.getJSONObject(index).toLocation()
        }
    }

    internal fun parseChangeResult(body: String): ChangeResult {
        val json = JSONObject(body)
        return ChangeResult(json.optString("tag").ifEmpty { null })
    }

    private fun JSONObject.toLocation() = Location(
            value = optString("value"),
            description = optString("description").ifEmpty { optString("value") },
            code = optString("code").ifEmpty { null },
            speed = optString("speed").ifEmpty { null },
            bestLocation = optBoolean("bestLocation"),
            systemLocation = optBoolean("systemLocation"),
    )

    private suspend fun request(url: URL) = withContext(Dispatchers.IO) {
        (url.openConnection() as HttpURLConnection).useCancellable {
            connectTimeout = 10_000
            readTimeout = 10_000
            setRequestProperty(PRODUCT_CODE_HEADER, PRODUCT_CODE_VALUE)
            when (responseCode) {
                HttpURLConnection.HTTP_OK -> inputStream.bufferedReader().use { it.readText() }
                else -> {
                    val detail = errorStream?.bufferedReader()?.use { it.readText() }?.takeIf { it.isNotBlank() }
                    throw ApiException(responseCode, detail)
                }
            }
        }
    }
}
