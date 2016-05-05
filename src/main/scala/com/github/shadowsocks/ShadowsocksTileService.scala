package com.github.shadowsocks

import android.content.Intent
import android.graphics.drawable.Icon
import android.service.quicksettings.{Tile, TileService}
import com.github.shadowsocks.aidl.IShadowsocksServiceCallback
import com.github.shadowsocks.utils.State

/**
  * @author Mygod
  */
final class ShadowsocksTileService extends TileService with ServiceBoundContext {
  private lazy val iconIdle = Icon.createWithResource(this, R.drawable.ic_start_idle).setTint(0x80ffffff)
  private lazy val iconBusy = Icon.createWithResource(this, R.drawable.ic_start_busy)
  private lazy val iconConnected = Icon.createWithResource(this, R.drawable.ic_start_connected)
  private lazy val callback = new IShadowsocksServiceCallback.Stub {
    def trafficUpdated(txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) = ()
    def stateChanged(state: Int, msg: String) {
      val tile = getQsTile
      state match {
        case State.STOPPED =>
          tile.setIcon(iconIdle)
          tile.setLabel(getString(R.string.app_name))
          tile.setState(Tile.STATE_INACTIVE)
        case State.CONNECTED =>
          tile.setIcon(iconConnected)
          tile.setLabel(ShadowsocksApplication.currentProfile match {
            case Some(profile) => profile.name
            case None => getString(R.string.app_name)
          })
          tile.setState(Tile.STATE_ACTIVE)
        case _ =>
          tile.setIcon(iconBusy)
          tile.setLabel(getString(R.string.app_name))
          tile.setState(Tile.STATE_UNAVAILABLE)
      }
      tile.updateTile
    }
  }

  override def onServiceConnected = callback.stateChanged(bgService.getState, null)

  override def onStartListening {
    super.onStartListening
    attachService(callback)
  }

  override def onStopListening {
    super.onStopListening
    detachService // just in case the user switches to NAT mode
  }

  override def onClick = if (isLocked) unlockAndRun(configure) else configure()

  private def configure() = startActivityAndCollapse(new Intent(this, classOf[Shadowsocks]))
}
