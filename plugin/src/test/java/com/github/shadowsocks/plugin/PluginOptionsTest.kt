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

package com.github.shadowsocks.plugin

import org.junit.Assert
import org.junit.Test

class PluginOptionsTest {
    @Test
    fun equal() {
        val o1 = PluginOptions("obfs-local;obfs=http;obfs-host=localhost")
        val o2 = PluginOptions("obfs-local", "obfs-host=localhost;obfs=http")
        Assert.assertEquals(o1.hashCode(), o2.hashCode())
        Assert.assertEquals(true, o1 == o2)
        val o3 = PluginOptions(o1.toString(false))
        Assert.assertEquals(true, o2 == o3)
        val o4 = PluginOptions(o2.id, o2.toString())
        Assert.assertEquals(true, o3 == o4)
    }
}
