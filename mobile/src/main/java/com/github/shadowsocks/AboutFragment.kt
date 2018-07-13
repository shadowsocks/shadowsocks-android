/*******************************************************************************
 *                                                                             *
 *  Copyright (C) 2017 by Max Lv <max.c.lv@gmail.com>                          *
 *  Copyright (C) 2017 by Mygod Studio <contact-shadowsocks-android@mygod.be>  *
 *                                                                             *
 *  This program is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation, either version 3 of the License, or          *
 *  (at your option) any later version.                                        *
 *                                                                             *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

package com.github.shadowsocks

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.text.Html
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import java.io.IOException
import java.io.InputStream

class AboutFragment : ToolbarFragment() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
            = inflater.inflate(R.layout.layout_about, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        toolbar.title = getString(R.string.about_title, BuildConfig.VERSION_NAME)
        val about = view.findViewById<TextView>(R.id.tv_about)
        setTextViewHTML(about, getHtml())
    }

    private fun setTextViewHTML(textView: TextView, html: String) {
        val seq = when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N -> Html.fromHtml(html, Html.FROM_HTML_MODE_LEGACY)
            else -> Html.fromHtml(html)
        }
        val stringBuilder = SpannableStringBuilder(seq)
        val urls = stringBuilder.getSpans(0, seq.length, URLSpan::class.java)
        for (span in urls) {
            makeLinkClickable(stringBuilder, span)
        }
        textView.text = stringBuilder
        textView.movementMethod = LinkMovementMethod.getInstance()
    }

    private fun makeLinkClickable(stringBuilder: SpannableStringBuilder, span: URLSpan) {
        val start = stringBuilder.getSpanStart(span)
        val end = stringBuilder.getSpanEnd(span)
        val flags = stringBuilder.getSpanFlags(span)
        val clickable = object : ClickableSpan() {
            override fun onClick(view: View) {
                if (span.url.startsWith("mailto:")) {
                    val intent = Intent().apply {
                        action = Intent.ACTION_SENDTO
                        data = Uri.parse(span.url)
                    }
                    startActivity(Intent.createChooser(intent, getString(R.string.send_email)))
                } else (activity as MainActivity).launchUrl(span.url)
            }
        }
        stringBuilder.setSpan(clickable, start, end, flags)
        stringBuilder.removeSpan(span)
    }

    private fun getHtml(): String {
        var `is`: InputStream? = null
        var buffer: ByteArray = byteArrayOf()
        try {
            `is` = requireContext().assets.open("pages/about.html")
            if (`is` != null) {
                val size = `is`.available()
                buffer = ByteArray(size)
                `is`.read(buffer)
            }
        } catch (e: IOException) {
            e.printStackTrace()
        } finally {
            if (`is` != null)
                try {
                    `is`.close()
                } catch (e: IOException) {
                    e.printStackTrace()
                }
        }
        return String(buffer)
    }
}