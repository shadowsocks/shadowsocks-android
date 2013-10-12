package be.jvb.iptypes

import java.util.Arrays
import scala.math.Ordered
import java.util

/**
 * Represents a byte array which is small enough to fit in a long (max 8 bytes).
 *
 * @author <a href="http://janvanbesien.blogspot.com">Jan Van Besien</a>
 */
private[iptypes] trait SmallByteArray extends Ordered[SmallByteArray] {
  val value: Long

  /** How many bytes do we use (max 8). */
  protected def nBytes: Int

  /** In what radix do we represent the bytes when converting to a string. */
  protected def radix: Radix

  /** Do we need to padd the bytes with zeros when converting to a string. */
  protected def zeroPaddingUpTo: Int = 0

  val maxValue = Math.pow(2, nBytes * 8).toLong - 1

  if (nBytes < 0 || nBytes > 8) {
    throw new IllegalArgumentException("SmallByteArray can be used for arrays of length 0 to 8 only")
  }

  if (value > maxValue || value < 0) {
    throw new IllegalArgumentException("out of range [0x" + java.lang.Long.toHexString(value) + "] with [" + nBytes + "] bytes")
  }

  /**
   * @return integer array of the bytes in this byte array.
   */
  def toIntArray: Array[Int] = {
    val ints = new Array[Int](nBytes)

    for (i <- 0 until nBytes) {
      ints(i) = (((value << i * 8) >>> 8 * (nBytes - 1)) & 0xFF).asInstanceOf[Int]
    }

    ints.foreach((anInt) => assert(anInt >= 0 && anInt <= 255))

    ints
  }

  /**
   * @return byte array of the bytes in this byte array.
   */
  def toByteArray: Array[Byte] = {
    val bytes = new Array[Byte](nBytes)

    for (i <- 0 until nBytes) {
      bytes(i) = (((value << i * 8) >>> 8 * (nBytes - 1)) & 0xFF).asInstanceOf[Byte]
    }

    bytes
  }

  override def compare(that: SmallByteArray): Int = {
    this.value.compare(that.value)
  }

  // equals implemented as suggested in staircase book

  override def equals(other: Any): Boolean = {
    other match {
      case that: SmallByteArray => that.canEqual(this) && this.value == that.value
      case _ => false
    }
  }

  protected def canEqual(other: Any): Boolean = {
    other.isInstanceOf[SmallByteArray]
  }

  override def hashCode = {
    value.hashCode
  }

  /**
   * @return String representation of the byte array, with "." between every byte.
   */
  override def toString: String = {
    val ints = toIntArray
    val strings = for (i <- 0 until nBytes) yield String.format(formatString, ints(i).asInstanceOf[Object])

    strings.mkString(".")
  }

  private lazy val formatString = {
    radix match {
      case HEX() => {
        if (zeroPaddingUpTo != 0)
          "%0" + zeroPaddingUpTo + "X"
        else
          "%X"
      }
      case DEC() => {
        if (zeroPaddingUpTo != 0)
          "%0" + zeroPaddingUpTo + "d"
        else
          "%d"
      }
    }
  }

}

object SmallByteArray {
  private[iptypes] def parseAsLong(string: String, length: Int, radix: Radix): Long = {
    if (string eq null)
      throw new IllegalArgumentException("can not parse [null]")

    val longArray = parseAsLongArray(string, radix)

    validate(longArray, length)
    mergeBytesOfArrayIntoLong(longArray)
  }

  private def parseAsLongArray(string: String, radix: Radix): Array[Long] = {
    string.split("\\.").map(java.lang.Long.parseLong(_, radix.radix))
  }

  private def validate(array: Array[Long], length: Int) = {
    if (array.length != length)
      throw new IllegalArgumentException("can not parse values [" + util.Arrays.toString(array) + "] into a SmallByteArray of [" + length + "] bytes")

    if (!array.filter(_ < 0).isEmpty)
      throw new IllegalArgumentException("each element should be positive [" + util.Arrays.toString(array) + "]")

    if (!array.filter(_ > 255).isEmpty)
      throw new IllegalArgumentException("each element should be less than 255 [" + util.Arrays.toString(array) + "]")
  }

  private def mergeBytesOfArrayIntoLong(array: Array[Long]): Long = {
    var result = 0L
    for (i <- 0 until array.length) {
      result |= (array(i) << ((array.length - i - 1) * 8))
    }
    result
  }
}
