package com.github.shadowsocks

import android.os.Bundle
import android.view.{LayoutInflater, View, ViewGroup}

class GlobalSettingsFragment extends ToolbarFragment {

  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    inflater.inflate(R.layout.layout_global_settings, container, false)

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar.setTitle(R.string.settings)
  }
}
