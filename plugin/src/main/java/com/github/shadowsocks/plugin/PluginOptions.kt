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

import java.util.*

/**
 * Helper class for processing plugin options.
 *
 * Based on: https://github.com/apache/ant/blob/588ce1f/src/main/org/apache/tools/ant/types/Commandline.java
 */
class PluginOptions : HashMap<String, String?> {
    var id = ""

    constructor() : super()
    constructor(initialCapacity: Int) : super(initialCapacity)
    constructor(initialCapacity: Int, loadFactor: Float) : super(initialCapacity, loadFactor)

    private constructor(options: String?, parseId: Boolean) : this() {
        @Suppress("NAME_SHADOWING")
        var parseId = parseId
        if (options.isNullOrEmpty()) return
        val tokenizer = StringTokenizer(options + ';', "\\=;", true)
        val current = StringBuilder()
        var key: String? = null
        while (tokenizer.hasMoreTokens()) {
            val nextToken = tokenizer.nextToken()
            when (nextToken) {
                "\\" -> current.append(tokenizer.nextToken())
                "=" -> if (key == null) {
                    key = current.toString()
                    current.setLength(0)
                } else current.append(nextToken)
                ";" -> {
                    if (key != null) {
                        put(key, current.toString())
                        key = null
                    } else if (current.isNotEmpty())
                        if (parseId) id = current.toString() else put(current.toString(), null)
                    current.setLength(0)
                    parseId = false
                }
                else -> current.append(nextToken)
            }
        }
    }

    constructor(options: String?) : this(options, true)
    constructor(id: String, options: String?) : this(options, false) {
        this.id = id
    }

    private fun append(result: StringBuilder, str: String) = (0 until str.length)
            .map { str[it] }
            .forEach {
                when (it) {
                    '\\', '=', ';' -> {
                        result.append('\\') // intentionally no break
                        result.append(it)
                    }
                    else -> result.append(it)
                }
            }

    fun toString(trimId: Boolean): String {
        val result = StringBuilder()
        if (!trimId) if (id.isEmpty()) return "" else append(result, id)
        for ((key, value) in entries) {
            if (result.isNotEmpty()) result.append(';')
            append(result, key)
            if (value != null) {
                result.append('=')
                append(result, value)
            }
        }
        return result.toString()
    }

    override fun toString(): String = toString(true)

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        return javaClass == other?.javaClass && super.equals(other) && id == (other as PluginOptions).id
    }
    override fun hashCode(): Int = Objects.hash(super.hashCode(), id)
}
