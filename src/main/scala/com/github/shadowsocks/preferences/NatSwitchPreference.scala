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

  override def onGetDefaultValue(a: TypedArray, index: Int) = {
    val result = ShadowsocksApplication.isVpnEnabled
    if (!ShadowsocksApplication.isRoot) {
      disabled = true
      setEnabled(false)
      setSummary(R.string.nat_summary_no_root)
    }
    result.asInstanceOf[AnyRef]
  }
}
