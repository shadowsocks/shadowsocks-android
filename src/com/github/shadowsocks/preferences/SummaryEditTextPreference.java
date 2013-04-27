package com.github.shadowsocks.preferences;

import android.content.Context;
import android.preference.EditTextPreference;
import android.util.AttributeSet;
import com.github.shadowsocks.R;

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
    public void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);
        if (positiveResult) {
            String value = getEditText().getText().toString();
            if (value.isEmpty()) {
                setSummary(mDefaultSummary);
            } else {
                setSummary(value);
            }
        }
    }
}
