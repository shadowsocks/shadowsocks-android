package com.github.shadowsocks.bg

/**
  * Shadowsocks service at its minimum.
  *
  * @author Mygod
  */
class ProxyService extends BaseService {
  val TAG = "ShadowsocksProxyService"

  def createNotification() = new ServiceNotification(this, profile.name, "service-proxy", true)
}
