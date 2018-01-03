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
import java.io.IOException
import java.io.InputStream
import java.util.concurrent.Semaphore

class GuardedProcess(private val cmd: List<String>) {
    companion object {
        private const val TAG = "GuardedProcess"
    }

    private lateinit var guardThread: Thread
    @Volatile
    private var isDestroyed = false
    @Volatile
    private lateinit var process: Process

    private fun streamLogger(input: InputStream, logger: (String, String) -> Int) = thread {
        try {
            input.bufferedReader().useLines { it.forEach { logger(TAG, it) } }
        } catch (_: IOException) { }    // ignore
    }

    fun start(onRestartCallback: (() -> Unit)? = null): GuardedProcess {
        val semaphore = Semaphore(1)
        semaphore.acquire()
        var ioException: IOException? = null
        guardThread = thread(name = "GuardThread-" + cmd.first()) {
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

                    semaphore.release()
                    process.waitFor()

                    synchronized(this) {
                        if (SystemClock.elapsedRealtime() - startTime < 1000) {
                            Log.w(TAG, "process exit too fast, stop guard: " + Commandline.toString(cmd))
                            isDestroyed = true
                        }
                    }
                }
            } catch (_: InterruptedException) {
                if (BuildConfig.DEBUG) Log.d(TAG, "thread interrupt, destroy process: " + Commandline.toString(cmd))
                destroyProcess()
            } catch (e: IOException) {
                ioException = e
            } finally {
                semaphore.release()
            }
        }
        semaphore.acquire()
        if (ioException != null) throw ioException!!
        return this
    }

    fun destroy() {
        isDestroyed = true
        guardThread.interrupt()
        destroyProcess()
        try {
            guardThread.join()
        } catch (_: InterruptedException) { }
    }

    private fun destroyProcess() {
        if (Build.VERSION.SDK_INT < 24) @Suppress("DEPRECATION") {
            JniHelper.sigtermCompat(process)
            JniHelper.waitForCompat(process, 500)
        }
        process.destroy()
    }
}
