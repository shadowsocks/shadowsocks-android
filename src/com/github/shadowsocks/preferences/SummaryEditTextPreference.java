package com.github.shadowsocks.preferences;

import android.content.Context;
import android.preference.EditTextPreference;
import android.util.AttributeSet;

public class SummaryEditTextPreference extends EditTextPreference {

  private CharSequence mDefaultSummary;

  public SummaryEditTextPreference(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
    mDefaultSummary = getSummary();
  }

  public SummaryEditTextPreference(Context context, AttributeSet attrs) {
    super(context, attrs);
    mDefaultSummary = getSummary();
  }

  public SummaryEditTextPreference(Context context) {
    super(context);
  }

  @Override
  public void setSummary(CharSequence summary) {
    if (summary.toString().isEmpty()) {
      super.setSummary(mDefaultSummary);
    } else {
      super.setSummary(summary);
    }
  }
}
