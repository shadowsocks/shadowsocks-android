package com.github.shadowsocks.net

import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import java.io.FileDescriptor
import java.io.IOException

abstract class SocketListener(name: String) : Thread(name), AutoCloseable {
    protected abstract val fileDescriptor: FileDescriptor
    @Volatile
    protected var running = true

    private fun FileDescriptor.shutdown() {
        // see also: https://issuetracker.google.com/issues/36945762#comment15
        if (valid()) try {
            Os.shutdown(this, OsConstants.SHUT_RDWR)
        } catch (e: ErrnoException) {
            // suppress fd inactive or already closed
            if (e.errno != OsConstants.EBADF && e.errno != OsConstants.ENOTCONN) throw IOException(e)
        }
    }

    override fun close() {
        running = false
        fileDescriptor.shutdown()
        join()
    }
}
