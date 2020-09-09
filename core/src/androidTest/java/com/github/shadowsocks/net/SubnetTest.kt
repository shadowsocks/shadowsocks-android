package com.github.shadowsocks.net

import android.net.InetAddresses.parseNumericAddress
import org.junit.Assert
import org.junit.Test
import java.net.InetAddress

class SubnetTest {
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

    @Test
    fun invalidParsings() {
        Assert.assertEquals(null, Subnet.fromString("1.2.3.456"))
        Assert.assertEquals(null, Subnet.fromString("ffff::f0000"))
        Assert.assertEquals(null, Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/129"))
        Assert.assertEquals(null, Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/129"))
        Assert.assertEquals(null, Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/-99"))
        Assert.assertEquals(null, Subnet.fromString("caec:cec6:c4ef:bb7b:1a78:d055:216d:3a78/1/0"))
    }

    @Test
    fun matching1() {
        val matcher = Subnet.fromString("1.10.11.12/25")!!.toImmutable()
        Assert.assertFalse(matcher.matches(parseNumericAddress("1.10.10.12").address))
        Assert.assertTrue(matcher.matches(parseNumericAddress("1.10.11.13").address))
        Assert.assertFalse(matcher.matches(parseNumericAddress("1.10.11.212").address))
    }

    @Test
    fun matching2() {
        val matcher = Subnet.fromString("14.208.0.0/12")!!.toImmutable()
        Assert.assertTrue(matcher.matches(parseNumericAddress("14.215.178.36").address))
        Assert.assertTrue(matcher.matches(parseNumericAddress("14.215.178.37").address))
        Assert.assertFalse(matcher.matches(parseNumericAddress("1.10.11.212").address))
    }
}
