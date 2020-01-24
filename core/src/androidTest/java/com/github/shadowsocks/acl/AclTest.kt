/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
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

import org.junit.Assert
import org.junit.Test

class AclTest {
    companion object {
        const val BYPASS_BASE = """[bypass_all]
[proxy_list]"""
        const val INPUT1 = """$BYPASS_BASE
1.0.1.0/24
2000::/8
(?:^|\.)4tern\.com${'$'}
"""
        const val INPUT2 = """[proxy_all]
[bypass_list]
10.3.0.0/16
10.0.0.0/8
(?:^|\.)chrome\.com${'$'}

[proxy_list]
# ignored
0.0.0.0/0
(?:^|\.)about\.google${'$'}
"""
    }

    @Test
    fun parse() {
        Assert.assertEquals(INPUT1, Acl().fromReader(INPUT1.reader()).toString())
    }
}
