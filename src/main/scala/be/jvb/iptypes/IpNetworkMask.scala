package be.jvb.iptypes

import java.lang.IllegalArgumentException

/**
 * Represents an IPv4 network mask.
 *
 * @author <a href="http://janvanbesien.blogspot.com">Jan Van Besien</a>
 */
class IpNetworkMask(override val value: Long) extends IpAddress(value) {
  def this(address: String) = this (SmallByteArray.parseAsLong(address, IpAddress.N_BYTES, DEC()))

  checkMaskValidity()

  private def checkMaskValidity() = {
    if (!IpNetworkMask.VALID_MASK_VALUES.contains(value))
      throw new IllegalArgumentException("Not a valid ip network mask [" + this + "]")
  }

  def prefixLength = {
    IpNetworkMask.fromLongToPrefixLength(value)
  }

}

object IpNetworkMask {
  private[iptypes] val VALID_MASK_VALUES = for (prefixLength <- 0 to 32) yield fromPrefixLenthToLong(
    prefixLength)

  /**
   * Convert a prefix length (e.g. 24) into a network mask (e.g. 255.255.255.0). IpNetworkMask hasn't got a public constructor for this, because
   * it would be confusing with the constructor that takes a long.
   */
  def fromPrefixLength(prefixLength: Int): IpNetworkMask = {
    new IpNetworkMask(fromPrefixLenthToLong(prefixLength))
  }

  private[iptypes] def fromPrefixLenthToLong(prefixLength: Int): Long = {
    (((1L << 32) - 1) << (32 - prefixLength)) & 0xFFFFFFFFL
  }

  private[iptypes] def fromLongToPrefixLength(value: Long): Int = {
    val lsb: Long = value & 0xFFFFFFFFL
    var result: Int = 0
    var bit: Long = 1L << 31

    while (((lsb & bit) != 0) && (result < 32)) {
      bit = bit >> 1
      result += 1
    }
    result
  }

  /**
   * Construct a network mask which has the longest matching prefix to still contain both given addresses.
   */
  def longestPrefixNetwork(first: IpAddress, last: IpAddress): IpNetworkMask = {
    IpNetworkMask.fromPrefixLength(IpNetworkMask.fromLongToPrefixLength(~first.value ^ last.value))
  }

  def apply(string: String): IpNetworkMask = new IpNetworkMask(SmallByteArray.parseAsLong(string, IpAddress.N_BYTES, DEC()))

  def unapply(ipNetworkMask: IpNetworkMask): Option[String] = {
    Some(ipNetworkMask.toString)
  }

}