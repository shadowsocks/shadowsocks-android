package com.github.shadowsocks.acl

import java.net.InetAddress

import com.github.shadowsocks.utils.Utils

/**
  * @author Mygod
  */
@throws[IllegalArgumentException]
class Subnet(val address: InetAddress, val prefixSize: Int) extends Comparable[Subnet] {
  private def addressLength = address.getAddress.length << 3
  if (prefixSize < 0 || prefixSize > addressLength) throw new IllegalArgumentException

  override def toString: String =
    if (prefixSize == addressLength) address.getHostAddress else address.getHostAddress + '/' + prefixSize

  override def compareTo(that: Subnet): Int = {
    val addrThis = address.getAddress
    val addrThat = that.address.getAddress
    var result = addrThis lengthCompare addrThat.length // IPv4 address goes first
    if (result != 0) return result
    for ((x, y) <- addrThis zip addrThat) {
      result = (x & 0xFF) compare (y & 0xFF)  // undo sign extension of signed byte
      if (result != 0) return result
    }
    prefixSize compare that.prefixSize
  }
}

object Subnet {
  @throws[IllegalArgumentException]
  def fromString(value: String): Subnet = {
    val parts = value.split("/")
    val addr = Utils.parseNumericAddress(parts(0))
    parts.length match {
      case 1 => new Subnet(addr, addr.getAddress.length << 3)
      case 2 => new Subnet(addr, parts(1).toInt)
      case _ => throw new IllegalArgumentException()
    }
  }
}
