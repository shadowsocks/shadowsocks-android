package com.github.shadowsocks.unrealvpn

import android.content.ActivityNotFoundException
import android.content.Context
import android.view.View
import androidx.browser.customtabs.CustomTabColorSchemeParams
import androidx.browser.customtabs.CustomTabsIntent
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import com.github.shadowsocks.R
import com.google.android.material.snackbar.Snackbar

class UiUtils(private val context: Context, private val snackbar: View) {

    private val customTabsIntent by lazy {
        CustomTabsIntent.Builder().apply {
            setColorScheme(CustomTabsIntent.COLOR_SCHEME_SYSTEM)
            setColorSchemeParams(
                CustomTabsIntent.COLOR_SCHEME_LIGHT,
                CustomTabColorSchemeParams.Builder().apply {
                    setToolbarColor(
                        ContextCompat.getColor(
                            context,
                            R.color.light_color_primary
                        )
                    )
                }.build()
            )
            setColorSchemeParams(
                CustomTabsIntent.COLOR_SCHEME_DARK,
                CustomTabColorSchemeParams.Builder().apply {
                    setToolbarColor(
                        ContextCompat.getColor(
                            context,
                            R.color.dark_color_primary
                        )
                    )
                }.build()
            )
        }.build()
    }

    fun snackbar(text: CharSequence = ""): Snackbar {
        return Snackbar.make(snackbar, text, Snackbar.LENGTH_LONG)
            .setTextMaxLines(10)
    }

    fun launchUrl(uri: String) {
        try {
            customTabsIntent.launchUrl(context, uri.toUri())
        } catch (_: ActivityNotFoundException) {
            snackbar(uri).show()
        }
    }

}
