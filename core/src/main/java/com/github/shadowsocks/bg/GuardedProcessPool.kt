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
import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import android.util.Log
import com.crashlytics.android.Crashlytics
import com.github.shadowsocks.Core
import com.github.shadowsocks.utils.Commandline
import com.github.shadowsocks.utils.thread
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.TimeUnit

class GuardedProcessPool {
    companion object Dummy : IOException("Oopsie the developer has made a no-no") {
        private const val TAG = "GuardedProcessPool"
        private val ProcessImpl by lazy { Class.forName("java.lang.ProcessManager\$ProcessImpl") }
        private val pid by lazy { ProcessImpl.getDeclaredField("pid").apply { isAccessible = true } }
        private val exitValueMutex by lazy {
            ProcessImpl.getDeclaredField("exitValueMutex").apply { isAccessible = true }
        }
    }

    private inner class Guard(private val cmd: List<String>, private val onRestartCallback: (() -> Unit)?) {
        val cmdName = File(cmd.first()).nameWithoutExtension
        val excQueue = ArrayBlockingQueue<IOException>(1)   // ArrayBlockingQueue doesn't want null
        private var pushed = false

        private fun streamLogger(input: InputStream, logger: (String, String) -> Int) =
                thread("StreamLogger-$cmdName") {
                    try {
                        input.bufferedReader().forEachLine { logger(TAG, it) }
                    } catch (_: IOException) { }    // ignore
                }
        private fun pushException(ioException: IOException?) {
            if (pushed) return
            excQueue.put(ioException ?: Dummy)
            pushed = true
        }

        fun looper(host: HashSet<Thread>) {
            var process: Process? = null
            try {
                var callback: (() -> Unit)? = null
                while (guardThreads.get() === host) {
                    Crashlytics.log(Log.DEBUG, TAG, "start process: " + Commandline.toString(cmd))
                    val startTime = SystemClock.elapsedRealtime()

                    process = ProcessBuilder(cmd)
                            .redirectErrorStream(true)
                            .directory(Core.deviceStorage.noBackupFilesDir)
                            .start()

                    streamLogger(process.inputStream, Log::i)
                    streamLogger(process.errorStream, Log::e)

                    if (callback == null) callback = onRestartCallback else callback()

                    pushException(null)
                    process.waitFor()

                    if (SystemClock.elapsedRealtime() - startTime < 1000) {
                        Crashlytics.log(Log.WARN, TAG, "process exit too fast, stop guard: $cmdName")
                        break
                    }
                }
            } catch (_: InterruptedException) {
                Crashlytics.log(Log.DEBUG, TAG, "thread interrupt, destroy process: $cmdName")
            } catch (e: IOException) {
                pushException(e)
            } finally {
                if (process != null) {
                    if (Build.VERSION.SDK_INT < 24) {
                        val pid = pid.get(process) as Int
                        try {
                            Os.kill(pid, OsConstants.SIGTERM)
                        } catch (e: ErrnoException) {
                            if (e.errno != OsConstants.ESRCH) throw e
                        }
                        val mutex = exitValueMutex.get(process) as Object
                        synchronized(mutex) {
                            try {
                                process.exitValue()
                            } catch (e: IllegalThreadStateException) {
                                mutex.wait(500)
                            }
                        }
                    }

                    process.destroy() // kill the process

                    if (Build.VERSION.SDK_INT >= 26) {
                        val isKilled = process.waitFor(1L, TimeUnit.SECONDS) // wait for 1 second
                        if (!isKilled) {
                            process.destroyForcibly() // Force to kill the process if it's still alive
                        }
                    }

                    process.waitFor()   // ensure the process is destroyed
                }
                pushException(null)
            }
        }
    }

    /**
     * This is an indication of which thread pool is being active.
     * Reading/writing this collection still needs an additional lock to prevent concurrent modification.
     */
    private val guardThreads = AtomicReference<HashSet<Thread>>(HashSet())

    fun start(cmd: List<String>, onRestartCallback: (() -> Unit)? = null): GuardedProcessPool {
        val guard = Guard(cmd, onRestartCallback)
        val guardThreads = guardThreads.get()
        synchronized(guardThreads) {
            guardThreads.add(thread("GuardThread-${guard.cmdName}") {
                guard.looper(guardThreads)
            })
        }
        val ioException = guard.excQueue.take()
        if (ioException !== Dummy) throw ioException
        return this
    }

    fun killAll() {
        val guardThreads = guardThreads.getAndSet(HashSet())
        synchronized(guardThreads) {
            guardThreads.forEach { it.interrupt() }
            try {
                guardThreads.forEach { it.join() }
            } catch (_: InterruptedException) { }
        }
    }
}
