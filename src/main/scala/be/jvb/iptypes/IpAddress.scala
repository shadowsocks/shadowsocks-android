package be.jvb.iptypes

import java.net.InetAddress

/**
 * Represents an IPv4 address.
 *
 * @author <a href="http://janvanbesien.blogspot.com">Jan Van Besien</a>
 *
 * @param value 32bit value of the ip address (only 32 least significant bits are used)
 */
class IpAddress(val value: Long) extends SmallByteArray {
  /**
   * Construct from a CIDR format string representation (e.g. "192.168.0.1").
   */
  def this(address: String) = this (SmallByteArray.parseAsLong(address, IpAddress.N_BYTES, DEC()))

  /**
   * Construct from a java.net.InetAddress.
   */
  def this(inetAddress: InetAddress) = {
    this (if (inetAddress == null) throw new IllegalArgumentException("can not create from [null]") else inetAddress
      .getHostAddress)
  }

  protected def nBytes = IpAddress.N_BYTES

  protected def radix = DEC()

  /**
   * Addition. Will never overflow, but wraps around when the highest ip address has been reached.
   */
  def +(value: Long) = new IpAddress((this.value + value) & maxValue)

  /**
   * Substraction. Will never underflow, but wraps around when the lowest ip address has been reached.
   */
  def -(value: Long) = new IpAddress((this.value - value) & maxValue)

  /**
   * Convert to java.net.InetAddress.
   */
  def toInetAddress: InetAddress = InetAddress.getByName(toString)

}

object IpAddress {
  val N_BYTES = 4

  def apply(string: String): IpAddress = new IpAddress(SmallByteArray.parseAsLong(string, N_BYTES, DEC()))

  def unapply(ipAddress: IpAddress): Option[String] = {
    Some(ipAddress.toString)
  }

}

