package com.github.shadowsocks.controllers

import android.content.Intent
import android.net.Uri
import android.support.v4.text.HtmlCompat
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.github.shadowsocks.BuildConfig
import com.github.shadowsocks.MainActivity
import com.github.shadowsocks.R
import com.github.shadowsocks.controllers.base.BaseController

class AboutController : BaseController() {
    companion object {
        const val TAG = "AboutController"
    }
    override fun inflateView(inflater: LayoutInflater, container: ViewGroup): View =
            inflater.inflate(R.layout.layout_about, container, false)
    override fun onViewBound(view: View) {
        super.onViewBound(view)
        view.findViewById<TextView>(R.id.tv_about).apply {
            text = SpannableStringBuilder(HtmlCompat.fromHtml(
                    resources.openRawResource(R.raw.about).bufferedReader().readText(),
                    HtmlCompat.FROM_HTML_SEPARATOR_LINE_BREAK_LIST_ITEM)).apply {
                for (span in getSpans(0, length, URLSpan::class.java)) {
                    setSpan(object : ClickableSpan() {
                        override fun onClick(view: View) {
                            if (span.url.startsWith("mailto:")) {
                                startActivity(Intent.createChooser(Intent().apply {
                                    action = Intent.ACTION_SENDTO
                                    data = Uri.parse(span.url)
                                }, resources!!.getString(R.string.send_email)))
                            } else (activity as MainActivity).launchUrl(span.url)
                        }
                    }, getSpanStart(span), getSpanEnd(span), getSpanFlags(span))
                    removeSpan(span)
                }
            }
            movementMethod = LinkMovementMethod.getInstance()
        }
    }

    override fun onAttach(view: View) {
        (activity as MainActivity).toolbar.title = resources!!.getString(R.string.about_title, BuildConfig.VERSION_NAME)
        super.onAttach(view)
    }
}