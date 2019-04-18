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

import android.content.Context
import android.util.Log
import androidx.recyclerview.widget.SortedList
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.Core
import com.github.shadowsocks.net.Subnet
import com.github.shadowsocks.preference.DataStore
import com.github.shadowsocks.utils.asIterable
import java.io.File
import java.io.IOException
import java.io.Reader
import java.net.URL
import java.net.URLConnection

class Acl {
    companion object {
        const val TAG = "Acl"
        const val ALL = "all"
        const val BYPASS_LAN = "bypass-lan"
        const val BYPASS_CHN = "bypass-china"
        const val BYPASS_LAN_CHN = "bypass-lan-china"
        const val GFWLIST = "gfwlist"
        const val CHINALIST = "china-list"
        const val CUSTOM_RULES = "custom-rules"

        val networkAclParser = "^IMPORT_URL\\s*<(.+)>\\s*$".toRegex()

        fun getFile(id: String, context: Context = Core.deviceStorage) = File(context.noBackupFilesDir, "$id.acl")

        var customRules: Acl
            get() {
                val acl = Acl()
                val str = DataStore.publicStore.getString(CUSTOM_RULES)
                if (str != null) acl.fromReader(str.reader(), true)
                if (!acl.bypass) {
                    acl.bypass = true
                    acl.subnets.clear()
                }
                return acl
            }
            set(value) = DataStore.publicStore.putString(CUSTOM_RULES,
                    if ((!value.bypass || value.subnets.size() == 0) && value.bypassHostnames.size() == 0 &&
                            value.proxyHostnames.size() == 0 && value.urls.size() == 0) null else value.toString())
        fun save(id: String, acl: Acl) = getFile(id).writeText(acl.toString())
    }

    private abstract class BaseSorter<T> : SortedList.Callback<T>() {
        override fun onInserted(position: Int, count: Int) { }
        override fun areContentsTheSame(oldItem: T?, newItem: T?): Boolean = oldItem == newItem
        override fun onMoved(fromPosition: Int, toPosition: Int) { }
        override fun onChanged(position: Int, count: Int) { }
        override fun onRemoved(position: Int, count: Int) { }
        override fun areItemsTheSame(item1: T?, item2: T?): Boolean = item1 == item2
        override fun compare(o1: T?, o2: T?): Int =
                if (o1 == null) if (o2 == null) 0 else 1 else if (o2 == null) -1 else compareNonNull(o1, o2)
        abstract fun compareNonNull(o1: T, o2: T): Int
    }
    private open class DefaultSorter<T : Comparable<T>> : BaseSorter<T>() {
        override fun compareNonNull(o1: T, o2: T): Int = o1.compareTo(o2)
    }
    private object StringSorter : DefaultSorter<String>()
    private object SubnetSorter : DefaultSorter<Subnet>()
    private object URLSorter : BaseSorter<URL>() {
        private val ordering = compareBy<URL>({ it.host }, { it.port }, { it.file }, { it.protocol })
        override fun compareNonNull(o1: URL, o2: URL): Int = ordering.compare(o1, o2)
    }

    val bypassHostnames = SortedList(String::class.java, StringSorter)
    val proxyHostnames = SortedList(String::class.java, StringSorter)
    val subnets = SortedList(Subnet::class.java, SubnetSorter)
    val urls = SortedList(URL::class.java, URLSorter)
    var bypass = false

    fun fromAcl(other: Acl): Acl {
        bypassHostnames.clear()
        for (item in other.bypassHostnames.asIterable()) bypassHostnames.add(item)
        proxyHostnames.clear()
        for (item in other.proxyHostnames.asIterable()) proxyHostnames.add(item)
        subnets.clear()
        for (item in other.subnets.asIterable()) subnets.add(item)
        urls.clear()
        for (item in other.urls.asIterable()) urls.add(item)
        bypass = other.bypass
        return this
    }
    fun fromReader(reader: Reader, defaultBypass: Boolean = false): Acl {
        bypassHostnames.clear()
        proxyHostnames.clear()
        subnets.clear()
        urls.clear()
        bypass = defaultBypass
        val bypassSubnets by lazy { SortedList(Subnet::class.java, SubnetSorter) }
        val proxySubnets by lazy { SortedList(Subnet::class.java, SubnetSorter) }
        var hostnames: SortedList<String>? = if (defaultBypass) proxyHostnames else bypassHostnames
        var subnets: SortedList<Subnet>? = if (defaultBypass) proxySubnets else bypassSubnets
        reader.useLines {
            for (line in it) {
                @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN")
                val blocks = (line as java.lang.String).split("#", 2)
                val url = networkAclParser.matchEntire(blocks.getOrElse(1) { "" })?.groupValues?.getOrNull(1)
                if (url != null) urls.add(URL(url))
                when (val input = blocks[0].trim()) {
                    "[outbound_block_list]" -> {
                        hostnames = null
                        subnets = null
                    }
                    "[black_list]", "[bypass_list]" -> {
                        hostnames = bypassHostnames
                        subnets = bypassSubnets
                    }
                    "[white_list]", "[proxy_list]" -> {
                        hostnames = proxyHostnames
                        subnets = proxySubnets
                    }
                    "[reject_all]", "[bypass_all]" -> bypass = true
                    "[accept_all]", "[proxy_all]" -> bypass = false
                    else -> if (subnets != null && input.isNotEmpty()) {
                        val subnet = Subnet.fromString(input)
                        if (subnet == null) hostnames!!.add(input) else subnets!!.add(subnet)
                    }
                }
            }
        }
        for (item in (if (bypass) proxySubnets else bypassSubnets).asIterable()) this.subnets.add(item)
        return this
    }

    fun fromId(id: String): Acl = try {
        fromReader(getFile(id).bufferedReader())
    } catch (_: IOException) { this }

    suspend fun flatten(depth: Int, connect: suspend (URL) -> URLConnection): Acl {
        if (depth > 0) for (url in urls.asIterable()) {
            val child = Acl()
            try {
                child.fromReader(connect(url).getInputStream().bufferedReader(), bypass).flatten(depth - 1, connect)
            } catch (e: IOException) {
                e.printStackTrace()
                continue
            }
            if (bypass != child.bypass) {
                Crashlytics.log(Log.WARN, TAG, "Imported network ACL has a conflicting mode set. " +
                        "This will probably not work as intended. URL: $url")
                child.subnets.clear() // subnets for the different mode are discarded
                child.bypass = bypass
            }
            for (item in child.bypassHostnames.asIterable()) bypassHostnames.add(item)
            for (item in child.proxyHostnames.asIterable()) proxyHostnames.add(item)
            for (item in child.subnets.asIterable()) subnets.add(item)
        }
        urls.clear()
        return this
    }

    override fun toString(): String {
        val result = StringBuilder()
        result.append(if (bypass) "[bypass_all]\n" else "[proxy_all]\n")
        val bypassList = (if (bypass) {
            bypassHostnames.asIterable().asSequence()
        } else {
            subnets.asIterable().asSequence().map(Subnet::toString) + bypassHostnames.asIterable().asSequence()
        }).toList()
        val proxyList = (if (bypass) {
            subnets.asIterable().asSequence().map(Subnet::toString) + proxyHostnames.asIterable().asSequence()
        } else {
            proxyHostnames.asIterable().asSequence()
        }).toList()
        if (bypassList.isNotEmpty()) {
            result.append("[bypass_list]\n")
            result.append(bypassList.joinToString("\n"))
            result.append('\n')
        }
        if (proxyList.isNotEmpty()) {
            result.append("[proxy_list]\n")
            result.append(proxyList.joinToString("\n"))
            result.append('\n')
        }
        result.append(urls.asIterable().joinToString("") { "#IMPORT_URL <$it>\n" })
        return result.toString()
    }
}
