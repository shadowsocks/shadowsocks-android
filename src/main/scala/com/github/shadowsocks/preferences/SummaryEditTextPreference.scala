package com.github.shadowsocks.preferences

import android.content.Context
import android.preference.EditTextPreference
import android.util.AttributeSet

class SummaryEditTextPreference(context: Context, attrs: AttributeSet, defStyle: Int)
  extends EditTextPreference(context, attrs, defStyle) {

  def this(context: Context, attrs: AttributeSet) = {
    this(context, attrs, android.R.attr.editTextPreferenceStyle)
    mDefaultSummary = getSummary
  }

  override def setSummary(summary: CharSequence) {
    if (summary.toString.isEmpty) {
      super.setSummary(mDefaultSummary)
    } else {
      super.setSummary(summary)
    }
  }

  private var mDefaultSummary: CharSequence = getSummary
}