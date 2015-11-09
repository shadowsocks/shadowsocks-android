package com.github.shadowsocks.preferences

import android.content.Context
import android.preference.Preference
import android.util.AttributeSet
import com.github.shadowsocks.R

/**
  * Make your preference support %s in summary. Override getSummaryValue to customize what to put in.
  * @author Mygod
  */
trait SummaryPreference extends Preference {
  private var mSummary: String = _

  protected def initSummary(context: Context, attrs: AttributeSet) {
    val a = context.obtainStyledAttributes(attrs, R.styleable.SummaryPreference)
    mSummary = a.getString(R.styleable.SummaryPreference_summary)
    a.recycle
  }

  protected def getSummaryValue: AnyRef

  /**
    * Returns the summary of this SummaryPreference. If the summary has a String formatting marker in it
    * (i.e. "%s" or "%1$s"), then the current entry value will be substituted in its place.
    *
    * @return the summary with appropriate string substitution
    */
  override def getSummary = {
    val entry = getSummaryValue
    if (mSummary == null || entry == null) super.getSummary else String.format(mSummary, entry)
  }
  /**
    * Sets the summary for this Preference with a CharSequence. If the summary has a String formatting marker in it
    * (i.e. "%s" or "%1$s"), then the current entry value will be substituted in its place when it's retrieved.
    *
    * @param summary The summary for the preference.
    */
  override def setSummary(summary: CharSequence) {
    super.setSummary(summary)
    if (summary == null && mSummary != null) mSummary = null
    else if (summary != null && summary != mSummary) mSummary = summary.toString
  }
}
