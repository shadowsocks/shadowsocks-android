/* Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
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

import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.system.ErrnoException;

public class JniHelper {
    static {
        System.loadLibrary("jni-helper");
    }

    @Deprecated // Use Process.destroy() since API 24
    public static void sigtermCompat(@NonNull Process process) throws Exception {
        if (Build.VERSION.SDK_INT >= 24) throw new UnsupportedOperationException("Never call this method in OpenJDK!");
        int errno = sigterm(process);
        if (errno != 0) throw new ErrnoException("kill", errno);
    }

    @Deprecated // only implemented for before API 24
    public static boolean waitForCompat(@NonNull Process process, long millis) throws Exception {
        if (Build.VERSION.SDK_INT >= 24) throw new UnsupportedOperationException("Never call this method in OpenJDK!");
        final Object mutex = getExitValueMutex(process);
        synchronized (mutex) {
            if (getExitValue(process) == null) mutex.wait(millis);
            return getExitValue(process) != null;
        }
    }

    public static native int sigkill(int pid);
    private static native int sigterm(Process process);
    private static native Integer getExitValue(Process process);
    private static native Object getExitValueMutex(Process process);
    public static native int sendFd(int fd, @NonNull String path);
    public static native void close(int fd);
    @Nullable
    public static native byte[] parseNumericAddress(@NonNull String str);
}
