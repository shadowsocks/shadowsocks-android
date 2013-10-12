package be.jvb.iptypes

import scala.math.Ordered

/**
 * Represents a continuous range of IPv4 ip addresses (bounds included).
 *
 * @author <a href="http://janvanbesien.blogspot.com">Jan Van Besien</a>
 *
 * @param first first address in the range
 * @param last last address in the range
 */
class IpAddressRange(val first: IpAddress, val last: IpAddress) extends Ordered[IpAddressRange] {
  if (last < first)
    throw new IllegalArgumentException("Cannot create ip address range with last address > first address")

  def contains(address: IpAddress): Boolean = {
    address >= first && address <= last
  }

  def contains(range: IpAddressRange): Boolean = {
    contains(range.first) && contains(range.last)
  }

  def overlaps(range: IpAddressRange): Boolean = {
    contains(range.first) || contains(range.last) || range.contains(first) || range.contains(last)
  }

  def length = {
    last.value - first.value + 1
  }

  def addresses(): Stream[IpAddress] = {
    if (first < last) {
      Stream.cons(first, new IpAddressRange(first + 1, last).addresses())
    } else {
      Stream.cons(first, Stream.empty)
    }
  }

  /**
   * Ip address ranges are ordered on the first address, or on the second address if the first is equal.
   */
  def compare(that: IpAddressRange): Int = {
    if (this.first != that.first)
      this.first.compare(that.first)
    else
      this.last.compare(that.last)
  }

  /**
   * Remove an address from the range, resulting in one, none or two new ranges.
   */
  def -(address: IpAddress): List[IpAddressRange] = {
    if (address eq null)
      throw new IllegalArgumentException("invalid address [null]")

    if (!contains(address))
      List(this)
    else if (address == first && address == last)
      List()
    else if (address == first)
      List(new IpAddressRange(first + 1, last))
    else if (address == last)
      List(new IpAddressRange(first, last - 1))
    else
      List(new IpAddressRange(first, address - 1), new IpAddressRange(address + 1, last))
  }

  /**
   * Extend the range just enough at its head or tail such that the given address is included.
   */
  def +(address: IpAddress): IpAddressRange = {
    if (address < first)
      new IpAddressRange(address, last)
    else if (address > last)
      new IpAddressRange(first, address)
    else
      this
  }

  override def toString: String = {
    first.toString + " - " + last.toString
  }

  // equals implemented as suggested in staircase book

  override def equals(other: Any): Boolean = {
    other match {
      case that: IpAddressRange => that.canEqual(this) && this.first == that.first && this.last == that.last
      case _ => false
    }
  }

  protected def canEqual(other: Any): Boolean = {
    other.isInstanceOf[IpAddressRange]
  }

  override def hashCode = {
    41 * (41 + first.hashCode) + last.hashCode
  }
}
