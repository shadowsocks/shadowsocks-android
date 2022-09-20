package com.github.shadowsocks.net

import android.net.Network
import android.os.Build
import androidx.annotation.RequiresApi
import okhttp3.*
import okhttp3.CacheControl.Companion.FORCE_NETWORK
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import org.xbill.DNS.EDNSOption
import org.xbill.DNS.Message
import org.xbill.DNS.Resolver
import org.xbill.DNS.TSIG
import java.io.IOException
import java.time.Duration
import java.util.*
import java.util.concurrent.CompletableFuture
import java.util.concurrent.CompletionStage
import java.util.concurrent.Executor
import java.util.concurrent.ExecutorService

class DohResolver(network: Network, host: String, maxConn: Int, private var timeout: Duration?, executor: ExecutorService) : Resolver {

    private val client: OkHttpClient
    private val uriTemplate: String
    override fun getTimeout(): Duration? {
        return this.timeout
    }

    init {
        uriTemplate = "https://$host/dns-query"
        val spec: ConnectionSpec = ConnectionSpec.MODERN_TLS
        val dispatcher = Dispatcher(executor)
        dispatcher.maxRequestsPerHost = maxConn
        this.client = OkHttpClient.Builder()
            .socketFactory(network.socketFactory)
            .connectionSpecs(listOf(spec))
            .dispatcher(dispatcher)
            .callTimeout(timeout!!)
            .build()
    }

    @RequiresApi(api = Build.VERSION_CODES.S)
    override fun sendAsync(query: Message, executor: Executor): CompletionStage<Message> {
        return this.sendAsync(query)
    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    override fun sendAsync(query: Message): CompletionStage<Message> {
//        println("OKHttp...$uriTemplate")
        val bytes = query.toWire()
        val body: RequestBody = bytes.toRequestBody(mediaType, 0, bytes.size)
        val request: Request = Request.Builder()
            .url(uriTemplate)
            .cacheControl(FORCE_NETWORK)
            .post(body)
            .build()
        val future = CompletableFuture<Message>()
        val call = client.newCall(request)
        call.enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                future.completeExceptionally(e)
            }

            @Throws(IOException::class)
            override fun onResponse(call: Call, response: Response) {
                response.use {
                    if (!it.isSuccessful) {
                        future.cancel(true)
                        return@use
                    }
                    it.body.use { responseBody ->
                        val msg = Message(responseBody.bytes())
                        future.complete(msg)
                    }
                }
            }
        })
        return future
    }

    override fun setPort(port: Int) {}
    override fun setTCP(flag: Boolean) {}
    override fun setIgnoreTruncation(flag: Boolean) {}
    override fun setEDNS(version: Int, payloadSize: Int, flags: Int, options: List<EDNSOption>) {}
    override fun setTSIGKey(key: TSIG) {}
    override fun setTimeout(timeout: Duration?) {
        this.timeout = timeout;
    }
//    override fun setTimeout(timeout: Duration?) {
//    }

    companion object {
        private val mediaType: MediaType = "application/dns-message".toMediaType()
    }
}