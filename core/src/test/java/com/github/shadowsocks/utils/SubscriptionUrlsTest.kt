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

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class SubscriptionUrlsTest {
    @Test
    fun keepsHttpsUntouched() {
        assertEquals("https://example.com/sub.json", SubscriptionUrls.parse("https://example.com/sub.json").toString())
    }

    @Test
    fun convertsSsconfToHttps() {
        assertEquals(
                "https://vvvvvv.click/vanya/3xxxe295",
                SubscriptionUrls.parse("ssconf://vvvvvv.click/vanya/3xxxe295").toString(),
        )
    }

    @Test
    fun extractsDynamicConfigFromHttpsUrl() {
        val source = SubscriptionUrls.parseDynamic(
                "https://vvvvvv.click/vanya/123e4567-e89b-12d3-a456-426614174000",
        )
        assertNotNull(source)
        assertEquals(
                "https://vvvvvv.click/app/v1/user/location/change?lang=ru-RU&token=123e4567-e89b-12d3-a456-426614174000&location=vienna",
                source!!.endpoint("/app/v1/user/location/change", "ru-RU", "vienna").toString(),
        )
    }

    @Test
    fun rejectsUrlsWithoutDynamicToken() {
        assertNull(SubscriptionUrls.parseDynamic("https://example.com/sub.json"))
    }
}
