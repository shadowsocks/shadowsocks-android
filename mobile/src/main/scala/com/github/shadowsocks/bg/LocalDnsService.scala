package com.github.shadowsocks.bg

import java.io.File
import java.net.Inet6Address

import com.github.shadowsocks.GuardedProcess
import com.github.shadowsocks.ShadowsocksApplication.app
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.utils.{IOUtils, Utils}
import org.json.{JSONArray, JSONObject}

import scala.collection.mutable.ArrayBuffer

/**
  * Shadowsocks service with local DNS.
  *
  * @author Mygod
  */
trait LocalDnsService extends BaseService {
  var overtureProcess: GuardedProcess = _

  override def startNativeProcesses() {
    super.startNativeProcesses()
    if (!profile.udpdns) overtureProcess = new GuardedProcess(buildAdditionalArguments(ArrayBuffer[String](
      new File(getApplicationInfo.nativeLibraryDir, Executable.OVERTURE).getAbsolutePath,
      "-c", buildOvertureConfig("overture.conf")
    )): _*).start()
  }

  private def buildOvertureConfig(file: String): String = {
    val config = new JSONObject()
      .put("BindAddress", "127.0.0.1:" + app.dataStore.portLocalDns)
      .put("RedirectIPv6Record", true)
      .put("DomainBase64Decode", true)
      .put("HostsFile", "hosts")
      .put("MinimumTTL", 3600)
      .put("CacheSize", 4096)
    def makeDns(name: String, address: String, edns: Boolean = true) = {
      val dns = new JSONObject()
        .put("Name", name)
        .put("Address", (Utils.parseNumericAddress(address) match {
          case _: Inet6Address => '[' + address + ']'
          case _ => address
        }) + ":53")
        .put("Timeout", 6)
        .put("EDNSClientSubnet", new JSONObject().put("Policy", "disable"))
      if (edns) dns
        .put("Protocol", "tcp")
        .put("Socks5Address", "127.0.0.1:" + app.dataStore.portProxy)
      else dns.put("Protocol", "udp")
      dns
    }
    val remoteDns = new JSONArray(profile.remoteDns.split(",").zipWithIndex.map {
      case (dns, i) => makeDns("UserDef-" + i, dns.trim)
    })
    val localDns = new JSONArray(Array(
      makeDns("Primary-1", "119.29.29.29", edns = false),
      makeDns("Primary-2", "114.114.114.114", edns = false)
    ))

    try {
      val localLinkDns = com.github.shadowsocks.utils.Dns.getDnsResolver(this)
      localDns.put(makeDns("Primary-3", localLinkDns, edns = false))
    } catch {
      case _: Exception => // Ignore
    }

    profile.route match {
      case Acl.BYPASS_CHN | Acl.BYPASS_LAN_CHN | Acl.GFWLIST | Acl.CUSTOM_RULES => config
        .put("PrimaryDNS", localDns)
        .put("AlternativeDNS", remoteDns)
        .put("IPNetworkFile", "china_ip_list.txt")
        .put("DomainFile", "gfwlist.txt")
      case Acl.CHINALIST => config
        .put("PrimaryDNS", localDns)
        .put("AlternativeDNS", remoteDns)
      case _ => config
        .put("PrimaryDNS", remoteDns)
        // no need to setup AlternativeDNS in Acl.ALL/BYPASS_LAN mode
        .put("OnlyPrimaryDNS", true)
    }
    IOUtils.writeString(new File(getFilesDir, file), config.toString)
    file
  }

  override def killProcesses() {
    super.killProcesses()
    if (overtureProcess != null) {
      overtureProcess.destroy()
      overtureProcess = null
    }
  }
}
