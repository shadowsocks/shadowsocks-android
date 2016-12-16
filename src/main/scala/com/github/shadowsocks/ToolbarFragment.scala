package com.github.shadowsocks

import android.app.Fragment
import android.os.Bundle
import android.support.v7.widget.Toolbar
import android.view.View

class ToolbarFragment extends Fragment {
  var toolbar: Toolbar = _

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar = view.findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    val activity = getActivity.asInstanceOf[MainActivity]
    activity.drawer.setToolbar(activity, toolbar, true)
  }
}
