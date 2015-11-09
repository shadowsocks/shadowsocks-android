package com.github.shadowsocks.preferences

import android.content.Context
import android.content.res.TypedArray
import android.preference.SwitchPreference
import android.util.AttributeSet
import com.github.shadowsocks.{R, ShadowsocksApplication}

/**
  * TODO: No animations?
  * @author Mygod
  */
final class NatSwitchPreference(context: Context, attrs: AttributeSet) extends SwitchPreference(context, attrs) {
  private var disabled = false

  override def isEnabled = super.isEnabled && !disabled
  override def setEnabled(b: Boolean) = super.setEnabled(b && !disabled)

  if (!ShadowsocksApplication.isRoot) {
    disabled = true
    setEnabled(false)
    setSummary(R.string.nat_summary_no_root)
  }

  override def onGetDefaultValue(a: TypedArray, index: Int) = ShadowsocksApplication.isVpnEnabled.asInstanceOf[AnyRef]
}
