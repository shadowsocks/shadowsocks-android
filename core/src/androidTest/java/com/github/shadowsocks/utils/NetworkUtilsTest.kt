package com.github.shadowsocks.utils

import org.junit.Assert
import org.junit.Test


class NetworkUtilsTest {
    @Test
    fun pingTest(){
       val result =  NetworkUtils.ping("127.0.0.1")
        Assert.assertEquals(result.type,true)
    }
}