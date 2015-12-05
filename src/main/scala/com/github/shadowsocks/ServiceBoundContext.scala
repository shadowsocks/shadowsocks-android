package com.github.shadowsocks

import android.content.{ComponentName, Context, Intent, ServiceConnection}
import android.os.{RemoteException, IBinder}
import com.github.shadowsocks.aidl.{IShadowsocksServiceCallback, IShadowsocksService}
import com.github.shadowsocks.utils.Action

/**
  * @author Mygod
  */
trait ServiceBoundContext extends Context {

  class ShadowsocksServiceConnection extends ServiceConnection {
    override def onServiceConnected(name: ComponentName, service: IBinder) {
      bgService = IShadowsocksService.Stub.asInterface(service)
      if (callback != null) try {
        bgService.registerCallback(callback)
      } catch {
        case ignored: RemoteException => // Nothing
      }
      ServiceBoundContext.this.onServiceConnected()
    }
    override def onServiceDisconnected(name: ComponentName) {
      if (callback != null) {
        try {
          if (bgService != null) bgService.unregisterCallback(callback)
        } catch {
          case ignored: RemoteException => // Nothing
        }
        callback = null
      }
      ServiceBoundContext.this.onServiceDisconnected()
      bgService = null
    }
  }

  def onServiceConnected() = ()
  def onServiceDisconnected() = ()

  private var callback: IShadowsocksServiceCallback.Stub = _
  private var connection: ShadowsocksServiceConnection = _

  // Variables
  var bgService: IShadowsocksService = _

  def attachService(callback: IShadowsocksServiceCallback.Stub = null) {
    this.callback = callback
    if (bgService == null) {
      val s =
        if (ShadowsocksApplication.isVpnEnabled) classOf[ShadowsocksVpnService] else classOf[ShadowsocksNatService]

      val intent = new Intent(this, s)
      intent.setAction(Action.SERVICE)

      connection = new ShadowsocksServiceConnection()
      bindService(intent, connection, Context.BIND_AUTO_CREATE)

      startService(new Intent(this, s))
    }
  }

  def deattachService() {
    if (bgService != null) {
      if (callback != null) {
        try {
          bgService.unregisterCallback(callback)
        } catch {
          case ignored: RemoteException => // Nothing
        }
      }
      if (connection != null) {
        unbindService(connection)
        connection = null
      }
    }
  }
}
