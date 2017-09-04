package com.github.shadowsocks.acl

import java.io.{File, FileNotFoundException}
import java.net.URL

import android.util.Log
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
  import Acl._

  @DatabaseField(generatedId = true)
  var id: Int = _
  val bypassHostnames = new mutable.SortedList[String]()
  val proxyHostnames = new mutable.SortedList[String]()
  val subnets = new mutable.SortedList[Subnet]()
  val urls = new mutable.SortedList[URL]()(urlOrdering)

  @DatabaseField
  var bypass: Boolean = _

  def getBypassHostnamesString: String = bypassHostnames.mkString("\n")
  def getProxyHostnamesString: String = proxyHostnames.mkString("\n")
  def getSubnetsString: String = subnets.mkString("\n")
  def getUrlsString: String = urls.mkString("\n")
  def setBypassHostnamesString(value: String) {
    bypassHostnames.clear()
    bypassHostnames ++= value.split("\n")
  }
  def setProxyHostnamesString(value: String) {
    proxyHostnames.clear()
    proxyHostnames ++= value.split("\n")
  }
  def setSubnetsString(value: String) {
    subnets.clear()
    subnets ++= value.split("\n").map(Subnet.fromString)
  }
  def setUrlsString(value: String) {
    urls.clear()
    urls ++= value.split("\n").map(new URL(_))
  }

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
  def fromSource(value: Source, defaultBypass: Boolean = false): Acl = {
    bypassHostnames.clear()
    proxyHostnames.clear()
    this.subnets.clear()
    urls.clear()
    bypass = defaultBypass
    lazy val bypassSubnets = new mutable.SortedList[Subnet]()
    lazy val proxySubnets = new mutable.SortedList[Subnet]()
    var hostnames: mutable.SortedList[String] = if (defaultBypass) proxyHostnames else bypassHostnames
    var subnets: mutable.SortedList[Subnet] = if (defaultBypass) proxySubnets else bypassSubnets
    for (line <- value.getLines()) (line.indexOf('#') match {
      case -1 => line
      case index =>
        line.substring(index + 1) match {
          case networkAclParser(url) => urls.add(new URL(url))
          case _ => // ignore
        }
        line.substring(0, index)  // trim comments
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
        case _: IllegalArgumentException => hostnames += input
      }
      case _ =>
    }
    this.subnets ++= (if (bypass) proxySubnets else bypassSubnets)
    this
  }
  final def fromId(id: String): Acl = fromSource(Source.fromFile(Acl.getFile(id)))

  def flatten(depth: Int): Acl = {
    if (depth > 0) for (url <- urls) {
      val child = new Acl().fromSource(Source.fromURL(url), bypass).flatten(depth - 1)
      if (bypass != child.bypass) {
        Log.w(TAG, "Imported network ACL has a conflicting mode set. This will probably not work as intended. URL: %s"
          .format(url))
        child.subnets.clear() // subnets for the different mode are discarded
        child.bypass = bypass
      }
      bypassHostnames ++= child.bypassHostnames
      proxyHostnames ++= child.proxyHostnames
      subnets ++= child.subnets
    }
    urls.clear()
    this
  }

  override def toString: String = {
    val result = new StringBuilder()
    result.append(if (bypass) "[bypass_all]\n" else "[proxy_all]\n")
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
    result.append(urls.map("#IMPORT_URL <%s>\n".format(_)).mkString)
    result.toString
  }

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
  final val TAG = "Acl"
  final val ALL = "all"
  final val BYPASS_LAN = "bypass-lan"
  final val BYPASS_CHN = "bypass-china"
  final val BYPASS_LAN_CHN = "bypass-lan-china"
  final val GFWLIST = "gfwlist"
  final val CHINALIST = "china-list"
  final val CUSTOM_RULES = "custom-rules"
  final val CUSTOM_RULES_FLATTENED = "custom-rules-flattened"
  private val networkAclParser = "^IMPORT_URL\\s*<(.+)>\\s*$".r

  private val urlOrdering: Ordering[URL] = Ordering.by(url => (url.getHost, url.getPort, url.getFile, url.getProtocol))

  def getFile(id: String) = new File(app.getFilesDir, id + ".acl")
  def customRules: Acl = {
    val acl = new Acl()
    try acl.fromId(CUSTOM_RULES) catch {
      case _: FileNotFoundException =>
    }
    acl.bypass = true
    acl.bypassHostnames.clear() // everything is bypassed
    acl
  }
  def save(id: String, acl: Acl): Unit = IOUtils.writeString(getFile(id), acl.toString)
}
