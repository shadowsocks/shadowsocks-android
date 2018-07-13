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
        return "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n" +
                "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n" +
                "    <head>\n" +
                "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n" +
                "    </head>\n" +
                "    <body style=\"font-family: Helvetica, arial, freesans, clean, sans-serif; font-size: 14px;line-height: 1.6; color: #333;padding: 20px; max-width: 960px; margin: 0 auto;\">\n" +
                "        <p>A <a href=\"http://shadowsocks.org\">Shadowsocks</a> client for Android, written in Kotlin.</p>\n" +
                "        <p>Copyright (C) 2017 by Max Lv <a href=\"&#x6d;&#97;&#x69;&#108;&#116;&#111;&#x3a;&#x6d;&#97;x&#46;&#99;.&#108;&#118;&#x40;&#103;&#109;&#97;&#x69;&#108;&#46;&#x63;&#x6f;&#109;\">&#x6d;&#97;&#x78;.&#x63;&#x2e;&#108;v&#64;&#x67;&#109;&#x61;&#x69;&#108;&#x2e;&#99;&#111;&#109;</a></p>\n" +
                "        <p>Copyright (C) 2017 by Mygod Studio <a href=\"mailto:contact-shadowsocks-android@mygod.be\">contact-shadowsocks-android@mygod.be</a></p>\n" +
                "        <p>This program is free software: you can redistribute it and/or modify\n" +
                "            it under the terms of the GNU General Public License as published by\n" +
                "            the Free Software Foundation, either version 3 of the License, or\n" +
                "            (at your option) any later version.</p>\n" +
                "        <p>This program is distributed in the hope that it will be useful,\n" +
                "            but WITHOUT ANY WARRANTY; without even the implied warranty of\n" +
                "            MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" +
                "            GNU General Public License for more details.</p>\n" +
                "        <p>You should have received a copy of the GNU General Public License\n" +
                "            along with this program. If not, see <a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>.</p>\n" +
                "        <h3 style=\"font-size: 18px\">Open Source Licenses</h3>\n" +
                "        <ul>\n" +
                "            <li>redsocks: <a href=\"https://github.com/shadowsocks/redsocks/blob/shadowsocks-android/README\">APL 2.0</a></li>\n" +
                "            <li>mbed TLS: <a href=\"https://github.com/ARMmbed/mbedtls/blob/development/LICENSE\">APL 2.0</a></li>\n" +
                "            <li>libevent: <a href=\"https://github.com/shadowsocks/libevent/blob/master/LICENSE\">BSD</a></li>\n" +
                "            <li>tun2socks: <a href=\"https://github.com/shadowsocks/badvpn/blob/shadowsocks-android/COPYING\">BSD</a></li>\n" +
                "            <li>pcre: <a href=\"https://android.googlesource.com/platform/external/pcre/+/master/dist2/LICENCE\">BSD</a></li>\n" +
                "            <li>libancillary: <a href=\"https://github.com/shadowsocks/libancillary/blob/shadowsocks-android/COPYING\">BSD</a></li>\n" +
                "            <li>shadowsocks-libev: <a href=\"https://github.com/shadowsocks/shadowsocks-libev/blob/master/LICENSE\">GPLv3</a></li>\n" +
                "            <li>overture: <a href=\"https://github.com/shawn1m/overture/blob/master/LICENSE\">MIT</a></li>\n" +
                "            <li>libev: <a href=\"https://github.com/shadowsocks/libev/blob/master/LICENSE\">GPLv2</a></li>\n" +
                "            <li>libsodium: <a href=\"https://github.com/jedisct1/libsodium/blob/master/LICENSE\">ISC</a></li>\n" +
                "        </ul>\n" +
                "    </body>\n" +
                "</html>\n"
    }
}