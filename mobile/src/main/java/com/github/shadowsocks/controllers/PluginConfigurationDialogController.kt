package com.github.shadowsocks.controllers

import android.support.v7.app.AlertDialog
import android.view.View
import android.widget.EditText
import com.github.shadowsocks.ProfileConfigActivity
import com.github.shadowsocks.plugin.PluginContract
import com.github.shadowsocks.plugin.PluginManager
import im.mash.preference.EditTextPreferenceDialogController

class PluginConfigurationDialogController : EditTextPreferenceDialogController() {

    companion object {
        const val PLUGIN_ID_FRAGMENT_TAG = "PluginConfigurationDialogController.PLUGIN_ID"
        fun newInstance(pluginConfig: String, pluginSelected: String): PluginConfigurationDialogController {
            val controller = PluginConfigurationDialogController()
            controller.args.apply {
                putString("key", pluginConfig)
                putString(PLUGIN_ID_FRAGMENT_TAG, pluginSelected)
            }
            return controller
        }
    }

    private lateinit var editText: EditText

    override fun onPrepareDialogBuilder(builder: AlertDialog.Builder) {
        super.onPrepareDialogBuilder(builder)
        val intent = PluginManager.buildIntent(args.getString(PLUGIN_ID_FRAGMENT_TAG), PluginContract.ACTION_HELP)
        val activity = activity!!
        if (intent.resolveActivity(activity.packageManager) != null) builder.setNeutralButton("?") { _, _ ->
            activity.startActivityForResult(intent.putExtra(PluginContract.EXTRA_OPTIONS, editText.text.toString()),
                    ProfileConfigActivity.REQUEST_CODE_PLUGIN_HELP)
        }
    }

    override fun onBindDialogView(view: View) {
        super.onBindDialogView(view)
        editText = view.findViewById(android.R.id.edit)
    }
}