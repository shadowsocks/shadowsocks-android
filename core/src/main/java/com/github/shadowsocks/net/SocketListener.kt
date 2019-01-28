package com.github.shadowsocks.net

import com.github.shadowsocks.utils.shutdown
import java.io.FileDescriptor

abstract class SocketListener(name: String) : Thread(name), AutoCloseable {
    protected abstract val fileDescriptor: FileDescriptor
    @Volatile
    protected var running = true

    override fun close() {
        running = false
        fileDescriptor.shutdown()
        join()
    }
}
