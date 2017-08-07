package com.github.shadowsocks.acl

import java.io.{File, FileNotFoundException, IOException}

import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.utils.IOUtils
import com.j256.ormlite.field.DatabaseField

import scala.collection.mutable
import scala.io.Source

/**
  * ACL handler compliant with: src/main/jni/shadowsocks-libev/src/acl.c
  *
  * OrmLite integration is unused for now.
  *
  * @author Mygod
  */
class Acl {
  @DatabaseField(generatedId = true)
  var id: Int = _
  val bypassHostnames = new mutable.SortedList[String]()
  val proxyHostnames = new mutable.SortedList[String]()
  val subnets = new mutable.SortedList[Subnet]()
  val urls = new mutable.SortedList[String]()
  @DatabaseField
  var bypass: Boolean = _

  def isUrl(url: String): Boolean = url.startsWith("http://") || url.startsWith("https://")

  def fromAcl(other: Acl): Acl = {
    bypassHostnames.clear()
    bypassHostnames ++= other.bypassHostnames
    proxyHostnames.clear()
    proxyHostnames ++= other.proxyHostnames
    subnets.clear()
    subnets ++= other.subnets
    urls.clear()
    urls ++= other.urls
    bypass = other.bypass
    this
  }
  def fromSource(value: Source, defaultBypass: Boolean = true): Acl = {
    bypassHostnames.clear()
    proxyHostnames.clear()
    this.subnets.clear()
    this.urls.clear()
    bypass = defaultBypass
    lazy val bypassSubnets = new mutable.SortedList[Subnet]()
    lazy val proxySubnets = new mutable.SortedList[Subnet]()
    var hostnames: mutable.SortedList[String] = if (defaultBypass) proxyHostnames else bypassHostnames
    var subnets: mutable.SortedList[Subnet] = if (defaultBypass) proxySubnets else bypassSubnets
    var in_urls = false
    for (line <- value.getLines()) (line.trim.indexOf('#') match {
       case 0 => {
         line.indexOf("NETWORK_ACL_BEGIN") match {
           case -1 =>
           case index => in_urls = true
         }
         line.indexOf("NETWORK_ACL_END") match {
           case -1 =>
           case index => in_urls = false
         }
         "" // ignore any comment lines
       }
       case index => if (!in_urls) line else ""
    }).trim match {
      case "[outbound_block_list]" =>
        hostnames = null
        subnets = null
      case "[black_list]" | "[bypass_list]" =>
        hostnames = bypassHostnames
        subnets = bypassSubnets
      case "[white_list]" | "[proxy_list]" =>
        hostnames = proxyHostnames
        subnets = proxySubnets
      case "[reject_all]" | "[bypass_all]" => bypass = true
      case "[accept_all]" | "[proxy_all]" => bypass = false
      case input if subnets != null && input.nonEmpty => try subnets += Subnet.fromString(input) catch {
        case _: IllegalArgumentException => if (isUrl(input)) {
          urls += input
        } else {
          hostnames += input
        }
      }
      case _ =>
    }
    this.subnets ++= (if (bypass) proxySubnets else bypassSubnets)
    this
  }
  final def fromId(id: String): Acl = fromSource(Source.fromFile(Acl.getFile(id)))

  def getAclString(network: Boolean): String = {
    val result = new StringBuilder()
    if (urls.nonEmpty) {
      result.append(urls.mkString("\n"))
      if (network) {
        result.append("\n#NETWORK_ACL_BEGIN\n")
        try {
          urls.foreach((url: String) => result.append(Source.fromURL(url).mkString))
        } catch {
          case e: IOException => // ignore
        }
        result.append("\n#NETWORK_ACL_END\n")
      }
    }
    if (result.isEmpty) {
      result.append(if (bypass) "[bypass_all]\n" else "[proxy_all]\n")
    }
    val (bypassList, proxyList) =
      if (bypass) (bypassHostnames.toStream, subnets.toStream.map(_.toString) #::: proxyHostnames.toStream)
      else (subnets.toStream.map(_.toString) #::: bypassHostnames.toStream, proxyHostnames.toStream)
    if (bypassList.nonEmpty) {
      result.append("[bypass_list]\n")
      result.append(bypassList.mkString("\n"))
      result.append('\n')
    }
    if (proxyList.nonEmpty) {
      result.append("[proxy_list]\n")
      result.append(proxyList.mkString("\n"))
      result.append('\n')
    }
    result.toString
  }

  override def toString: String = {
    getAclString(false)
  }

  def isValidCustomRules: Boolean = bypass && bypassHostnames.isEmpty

  // Don't change: dummy fields for OrmLite interaction

  // noinspection ScalaUnusedSymbol
  @DatabaseField(useGetSet = true)
  private val bypassHostnamesString: String = null
  // noinspection ScalaUnusedSymbol
  @DatabaseField(useGetSet = true)
  private val proxyHostnamesString: String = null
  // noinspection ScalaUnusedSymbol
  @DatabaseField(useGetSet = true)
  private val subnetsString: String = null
}

object Acl {
  final val ALL = "all"
  final val BYPASS_LAN = "bypass-lan"
  final val BYPASS_CHN = "bypass-china"
  final val BYPASS_LAN_CHN = "bypass-lan-china"
  final val GFWLIST = "gfwlist"
  final val CHINALIST = "china-list"
  final val CUSTOM_RULES = "custom-rules"

  def getFile(id: String) = new File(app.getFilesDir, id + ".acl")
  def customRules: Acl = {
    val acl = new Acl()
    try acl.fromId(CUSTOM_RULES) catch {
      case _: FileNotFoundException =>
    }
    acl
  }
  def save(id: String, acl: Acl, network: Boolean = false): Unit = {
    IOUtils.writeString(getFile(id), acl.getAclString(network))
  }
}
