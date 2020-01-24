/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2020 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2020 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks.acl

import androidx.test.core.app.ApplicationProvider
import com.github.shadowsocks.Core
import com.github.shadowsocks.utils.parseNumericAddress
import kotlinx.coroutines.runBlocking
import org.junit.Assert
import org.junit.BeforeClass
import org.junit.Test

class AclMatcherTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun setup() {
            Core.app = ApplicationProvider.getApplicationContext()
        }
    }

    @Test
    fun emptyFile() {
        runBlocking {
            AclMatcher().apply {
                init(AclTest.BYPASS_BASE.reader())
                Assert.assertTrue(shouldBypassIpv4(ByteArray(4)))
                Assert.assertTrue(shouldBypassIpv6(ByteArray(16)))
                Assert.assertNull(shouldBypass("www.google.com"))
            }
        }
    }

    @Test
    fun basic() {
        runBlocking {
            AclMatcher().apply {
                init(AclTest.INPUT1.reader())
                Assert.assertTrue(shouldBypassIpv4("0.1.2.3".parseNumericAddress()!!.address))
                Assert.assertFalse(shouldBypassIpv4("1.0.1.2".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypassIpv4("1.0.3.2".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypassIpv6("::".parseNumericAddress()!!.address))
                Assert.assertFalse(shouldBypassIpv6("2020::2020".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypassIpv6("fe80::2020".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypass("4tern.com") == false)
                Assert.assertTrue(shouldBypass("www.4tern.com") == false)
                Assert.assertNull(shouldBypass("www.google.com"))
            }
        }
    }

    @Test
    fun bypassList() {
        runBlocking {
            AclMatcher().apply {
                init(AclTest.INPUT2.reader())
                Assert.assertFalse(shouldBypassIpv4("0.1.2.3".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypassIpv4("10.0.1.2".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypassIpv4("10.10.1.2".parseNumericAddress()!!.address))
                Assert.assertFalse(shouldBypassIpv4("11.0.1.2".parseNumericAddress()!!.address))
                Assert.assertTrue(shouldBypass("chrome.com") == true)
                Assert.assertTrue(shouldBypass("about.google") == false)
                Assert.assertNull(shouldBypass("www.google.com"))
            }
        }
    }
}
