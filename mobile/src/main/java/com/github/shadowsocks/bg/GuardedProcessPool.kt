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

package com.github.shadowsocks.bg

import android.os.Build
import android.os.SystemClock
import android.util.Log
import com.github.shadowsocks.App.Companion.app
import com.github.shadowsocks.BuildConfig
import com.github.shadowsocks.JniHelper
import com.github.shadowsocks.utils.Commandline
import com.github.shadowsocks.utils.thread
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.*
import java.util.concurrent.ArrayBlockingQueue

class GuardedProcessPool {
    companion object {
        private const val TAG = "GuardedProcessPool"
        private val dummy = IOException()
    }

    private inner class Guard(private val cmd: List<String>, private val onRestartCallback: (() -> Unit)?) {
        val cmdName = File(cmd.first()).nameWithoutExtension
        val excQueue = ArrayBlockingQueue<IOException>(1)   // ArrayBlockingQueue doesn't want null
        private var pushed = false

        private fun streamLogger(input: InputStream, logger: (String, String) -> Int) =
                thread("StreamLogger-$cmdName") {
                    try {
                        input.bufferedReader().useLines { it.forEach { logger(TAG, it) } }
                    } catch (_: IOException) { }    // ignore
                }
        private fun pushException(ioException: IOException?) {
            if (pushed) return
            excQueue.put(ioException ?: dummy)
            pushed = true
        }

        fun looper() {
            var process: Process? = null
            try {
                var callback: (() -> Unit)? = null
                while (!isDestroyed) {
                    if (BuildConfig.DEBUG) Log.d(TAG, "start process: " + Commandline.toString(cmd))
                    val startTime = SystemClock.elapsedRealtime()

                    process = ProcessBuilder(cmd)
                            .redirectErrorStream(true)
                            .directory(app.deviceContext.filesDir)
                            .start()

                    streamLogger(process.inputStream, Log::i)
                    streamLogger(process.errorStream, Log::e)

                    if (callback == null) callback = onRestartCallback else callback()

                    pushException(null)
                    process.waitFor()

                    synchronized(this) {
                        if (SystemClock.elapsedRealtime() - startTime < 1000) {
                            Log.w(TAG, "process exit too fast, stop guard: $cmdName")
                            isDestroyed = true
                        }
                    }
                }
            } catch (_: InterruptedException) {
                if (BuildConfig.DEBUG) Log.d(TAG, "thread interrupt, destroy process: $cmdName")
            } catch (e: IOException) {
                pushException(e)
            } finally {
                if (process != null) {
                    if (Build.VERSION.SDK_INT < 24) @Suppress("DEPRECATION") {
                        JniHelper.sigtermCompat(process)
                        JniHelper.waitForCompat(process, 500)
                    }
                    process.destroy()
                    process.waitFor()   // ensure the process is destroyed
                }
                pushException(null)
            }
        }
    }

    private val guardThreads = HashSet<Thread>()
    @Volatile
    private var isDestroyed = false

    fun start(cmd: List<String>, onRestartCallback: (() -> Unit)? = null): GuardedProcessPool {
        val guard = Guard(cmd, onRestartCallback)
        guardThreads.add(thread("GuardThread-${guard.cmdName}", block = guard::looper))
        val ioException = guard.excQueue.take()
        if (ioException !== dummy) throw ioException
        return this
    }

    fun killAll() {
        isDestroyed = true
        guardThreads.forEach { it.interrupt() }
        try {
            guardThreads.forEach { it.join() }
        } catch (_: InterruptedException) { }

        guardThreads.clear()
        isDestroyed = false
    }
}
