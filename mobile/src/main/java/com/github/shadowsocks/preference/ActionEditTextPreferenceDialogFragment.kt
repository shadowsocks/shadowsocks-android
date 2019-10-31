package com.github.shadowsocks.preference

import android.app.Dialog
import android.view.View
import android.widget.Button
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.core.os.bundleOf
import androidx.preference.EditTextPreferenceDialogFragmentCompat
import com.github.shadowsocks.R
import com.github.shadowsocks.acl.Acl
import com.github.shadowsocks.utils.printLog
import com.github.shadowsocks.utils.useCancellable
import kotlinx.coroutines.*
import java.net.HttpURLConnection
import java.net.URL


class ActionEditTextPreferenceDialogFragment : EditTextPreferenceDialogFragmentCompat() {
    private lateinit var mEditText: EditText
    private lateinit var mButtonNeutral: Button
    private lateinit var mButtonPositive: Button
    private lateinit var mButtonNegative: Button

    override fun onStart() {
        super.onStart()

        isCancelable = false
        val d = requireDialog() as AlertDialog
        mButtonNeutral = d.getButton(Dialog.BUTTON_NEUTRAL)
        mButtonNegative = d.getButton(Dialog.BUTTON_NEGATIVE)
        mButtonPositive = d.getButton(Dialog.BUTTON_POSITIVE)

        mButtonNeutral.apply {
            setOnClickListener {
                var success = true
                mButtonPositive.isEnabled = false
                mButtonNegative.isEnabled = false
                mButtonNeutral.isEnabled = false
                GlobalScope.launch(Dispatchers.IO) {
                    try {
                        withTimeout(10000L) {
                            val connection = URL(mEditText.text.toString()).openConnection() as HttpURLConnection
                            val acl = connection.useCancellable { inputStream.bufferedReader().use { it.readText() } }
                            Acl.getFile(Acl.CUSTOM_RULES).printWriter().use { it.write(acl) }
                        }
                    } catch (e: Exception) {
                        printLog(e)
                        success = false
                    }
                    withContext(Dispatchers.Main.immediate) {
                        mButtonPositive.isEnabled = true
                        mButtonNegative.isEnabled = true

                        if (success) {
                            isEnabled = false
                            text = getString(R.string.update_success)
                        } else text = getString(R.string.update_fail)
                    }
                }
            }
        }
    }

    fun setKey(key: String) {
        arguments = bundleOf(Pair(ARG_KEY, key))
    }

    override fun onBindDialogView(view: View) {
        super.onBindDialogView(view)
        mEditText = view.findViewById(android.R.id.edit)
    }

    override fun onPrepareDialogBuilder(builder: AlertDialog.Builder) {
        super.onPrepareDialogBuilder(builder)
        builder.setNeutralButton(R.string.update, null)
    }

}
