package com.github.shadowsocks.utils

import com.github.shadowsocks.utils.Subnet
import org.junit.Assert
import org.junit.Test
import java.net.InetAddress

object SubnetTest {
    @Test
    fun parsingAndEquals() {
        Assert.assertEquals(Subnet(InetAddress.getByName("1.10.11.12"), 25), Subnet.fromString("1.10.11.12/25"))
        Assert.assertEquals(Subnet(InetAddress.getByName("12.118.130.86"), 32), Subnet.fromString("12.118.130.86"))
        Assert.assertEquals(Subnet(InetAddress.getByName("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78"), 96),
                Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/96"))
        Assert.assertEquals(Subnet(InetAddress.getByName("be37:44bd:8630:7a0:2a3d:ff95:dd33:42f0"), 128),
                Subnet.fromString("be37:44bd:8630:7a0:2a3d:ff95:dd33:42f0"))
        Assert.assertNotEquals(Subnet.fromString("1.2.3.4/12"), Subnet.fromString("1.2.3.5/12"))
    }

    @Test(expected = IllegalArgumentException::class)
    fun invalidParsing1() {
        Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/129")
    }
    @Test(expected = IllegalArgumentException::class)
    fun invalidParsing2() {
        Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/-99")
    }
    @Test(expected = IllegalArgumentException::class)
    fun invalidParsing3() {
        Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/1/0")
    }
}
