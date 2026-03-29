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

package com.github.shadowsocks.utils

import java.net.MalformedURLException
import java.net.URI
import java.net.URL
import java.net.URLEncoder
import java.util.Locale

object SubscriptionUrls {
    private const val SSCONF_SCHEME = "ssconf"
    private val tokenPattern =
            Regex("""[0-9a-f]{8}-[0-9a-f]{4}-[0-5][0-9a-f]{3}-[089ab][0-9a-f]{3}-[0-9a-f]{12}""",
                    RegexOption.IGNORE_CASE)

    data class DynamicConfigSource(val url: URL, val token: String) {
        private val authority = if (url.port == -1 || url.port == url.defaultPort) url.host else "${url.host}:${url.port}"

        fun endpoint(path: String, languageTag: String, location: String? = null): URL {
            val params = buildList {
                add("lang=${encode(languageTag)}")
                add("token=${encode(token)}")
                if (location != null) add("location=${encode(location)}")
            }
            return URL("https://$authority$path?${params.joinToString("&")}")
        }

        private fun encode(value: String) = URLEncoder.encode(value, "UTF-8")
    }

    fun parse(raw: String): URL {
        val uri = try {
            URI(raw.trim())
        } catch (e: Exception) {
            throw MalformedURLException(e.message)
        }
        return when (uri.scheme?.lowercase(Locale.ENGLISH)) {
            "http", "https" -> uri.toURL()
            SSCONF_SCHEME -> URI(
                    "https",
                    uri.userInfo,
                    uri.host ?: uri.authority ?: throw MalformedURLException("Missing host"),
                    uri.port,
                    uri.path,
                    uri.query,
                    uri.fragment,
            ).toURL()
            else -> throw MalformedURLException("unknown protocol: ${uri.scheme ?: ""}")
        }
    }

    fun parseDynamic(raw: String): DynamicConfigSource? {
        val url = parse(raw)
        val token = tokenPattern.find(raw)?.value ?: tokenPattern.find(url.toString())?.value ?: return null
        return DynamicConfigSource(url, token)
    }
}
