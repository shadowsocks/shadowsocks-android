package com.github.shadowsocks.acl

import java.net.{Inet4Address, Inet6Address, InetAddress}

import com.github.shadowsocks.utils.Utils

/**
  * @author Mygod
  */
@throws[IllegalArgumentException]
class Subnet(val address: InetAddress, val prefixSize: Int) extends Comparable[Subnet] {
  if (prefixSize < 0) throw new IllegalArgumentException
  address match {
    case _: Inet4Address => if (prefixSize > 32) throw new IllegalArgumentException
    case _: Inet6Address => if (prefixSize > 128) throw new IllegalArgumentException
  }

  override def toString: String = if (address match {
    case _: Inet4Address => prefixSize == 32
    case _: Inet6Address => prefixSize == 128
  }) address.toString else address.toString + '/' + prefixSize

  override def compareTo(that: Subnet): Int = {
    val addrThis = address.getAddress
    val addrThat = that.address.getAddress
    var result = addrThis lengthCompare addrThat.length // IPv4 address goes first
    if (result != 0) return result
    for ((x, y) <- addrThis zip addrThat) {
      result = x compare y
      if (result != 0) return result
    }
    prefixSize compare that.prefixSize
  }
}

object Subnet {
  @throws[IllegalArgumentException]
  def fromString(value: String): Subnet = {
    val parts = value.split("/")
    if (!Utils.isNumeric(parts(0))) throw new IllegalArgumentException()
    val addr = InetAddress.getByName(parts(0))
    parts.length match {
      case 1 => new Subnet(addr, addr match {
        case _: Inet4Address => 32
        case _: Inet6Address => 128
      })
      case 2 => new Subnet(addr, parts(1).toInt)
      case _ => throw new IllegalArgumentException()
    }
  }
}
