package be.jvb.iptypes

import java.lang.String

/**
 * Represents an Ipv4 network (i.e. an address and a mask).
 *
 * @author <a href="http://janvanbesien.blogspot.com">Jan Van Besien</a>
 *
 * @param address network address of the network
 * @param mask network mask of the network
 */
class IpNetwork(val address: IpAddress, val mask: IpNetworkMask)
        extends IpAddressRange(IpNetwork.first(address, mask), IpNetwork.last(address, mask)) {
  /**
   * Construct a network from two addresses. This will create the smallest possible network ("longest prefix match") which contains
   * both addresses.
   */
  def this(first: IpAddress, last: IpAddress) = this (first, IpNetworkMask.longestPrefixNetwork(first, last))

  private def this(addressAndMask: (IpAddress, IpNetworkMask)) = this (addressAndMask._1, addressAndMask._2)

  /**
   * Construct a network from a CIDR notation (e.g. "192.168.0.0/24" or "192.168.0.0/255.255.255.0")
   */
  def this(network: String) = this (IpNetwork.parseAddressAndMaskFromCidrNotation(network))

  /**
   * @return CIDR notation
   */
  override def toString: String = first.toString + "/" + mask.prefixLength
}

object IpNetwork {
  /**
   * get the first address from a network which contains the given address.
   */
  private[iptypes] def first(address: IpAddress, mask: IpNetworkMask): IpAddress = {
    new IpAddress(address.value & mask.value)
  }

  /**
   * get the last address from a network which contains the given address.
   */
  private[iptypes] def last(address: IpAddress, mask: IpNetworkMask): IpAddress = {
    first(address, mask) + (0xFFFFFFFFL >> mask.prefixLength)
  }

  private[iptypes] def parseAddressAndMaskFromCidrNotation(cidrString: String): (IpAddress, IpNetworkMask) = {
    if (!cidrString.contains("/"))
      throw new IllegalArgumentException("no CIDR format [" + cidrString + "]")

    val addressAndMask: (String, String) = splitInAddressAndMask(cidrString)

    val address = new IpAddress(addressAndMask._1)
    val mask = parseNetworkMask(addressAndMask._2)

    (address, mask)
  }

  private def splitInAddressAndMask(cidrString: String): (String, String) = {
    val addressAndMask: Array[String] = cidrString.split("/")
    if (addressAndMask.length != 2)
      throw new IllegalArgumentException("no CIDR format [" + cidrString + "]")
    (addressAndMask(0), addressAndMask(1))
  }

  private def parseNetworkMask(mask: String) = {
    try
    {
      if (mask.contains("."))
        new IpNetworkMask(mask)
      else
        IpNetworkMask.fromPrefixLength(Integer.parseInt(mask))
    } catch {
      case e: Exception => throw new IllegalArgumentException("not a valid network mask [" + mask + "]", e)
    }
  }
}