package com.github.shadowsocks
import java.util.Locale

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.{LayoutInflater, View, ViewGroup}
import android.webkit.{WebView, WebViewClient}

class AboutFragment extends ToolbarFragment {
  override def onCreateView(inflater: LayoutInflater, container: ViewGroup, savedInstanceState: Bundle): View =
    inflater.inflate(R.layout.layout_about, container, false)

  override def onViewCreated(view: View, savedInstanceState: Bundle) {
    super.onViewCreated(view, savedInstanceState)
    toolbar.setTitle(getString(R.string.about_title).formatLocal(Locale.ENGLISH, BuildConfig.VERSION_NAME))
    val web = view.findViewById(R.id.web_view).asInstanceOf[WebView]
    web.loadUrl("file:///android_asset/pages/about.html")
    web.setWebViewClient(new WebViewClient() {
      override def shouldOverrideUrlLoading(view: WebView, url: String): Boolean = {
        try startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))) catch {
          case _: android.content.ActivityNotFoundException => // Ignore
        }
        true
      }
    })
  }
}
