/*
 * Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2015 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks;

import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.System;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

public class GuardedProcess extends Process {
    private static final String TAG = GuardedProcess.class.getSimpleName();
    private final Thread guardThread;
    private volatile boolean isDestroyed = false;
    private volatile Process process = null;

    public GuardedProcess(String... cmd) throws InterruptedException, IOException {
        this(Arrays.asList(cmd));
    }

    public GuardedProcess(final List<String> cmd) throws InterruptedException, IOException {
        this(cmd, null);
    }

    public GuardedProcess(final List<String> cmd, final Runnable onRestartCallback) throws InterruptedException, IOException {
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        final AtomicReference<IOException> atomicIoException = new AtomicReference<IOException>(null);
        guardThread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    while (!isDestroyed) {
                        Log.i(TAG, "start process: " + cmd);
                        long startTime = System.currentTimeMillis();
                        process = startProcess(cmd);
                        if (onRestartCallback != null && countDownLatch.getCount() <= 0) {
                            onRestartCallback.run();
                        }
                        countDownLatch.countDown();
                        process.waitFor();
                        if (System.currentTimeMillis() - startTime < 1000) {
                            Log.w(TAG, "process exit too fast, stop guard: " + cmd);
                            break;
                        }
                    }
                } catch (InterruptedException ignored) {
                    Log.i(TAG, "thread interrupt, destroy process: " + cmd);
                    process.destroy();
                } catch (IOException e) {
                    atomicIoException.compareAndSet(null, e);
                } finally {
                    countDownLatch.countDown();
                }
            }
        }, "GuardThread-" + cmd);
        guardThread.start();
        countDownLatch.await();
        IOException ioException = atomicIoException.get();
        if (ioException != null) {
            throw ioException;
        }
    }

    private static Process startProcess(List<String> cmd) throws IOException {
        return new ProcessBuilder(cmd).redirectErrorStream(true).start();
    }

    public void destroy() {
        isDestroyed = true;
        guardThread.interrupt();
        process.destroy();
        try {
            guardThread.join();
        } catch (InterruptedException ignored) {
        }
    }

    @Override
    public int exitValue() {
        throw new UnsupportedOperationException();
    }

    @Override
    public InputStream getErrorStream() {
        throw new UnsupportedOperationException();
    }

    @Override
    public InputStream getInputStream() {
        throw new UnsupportedOperationException();
    }

    @Override
    public OutputStream getOutputStream() {
        throw new UnsupportedOperationException();
    }

    @Override
    public int waitFor() throws InterruptedException {
        guardThread.join();
        return 0;
    }
}
