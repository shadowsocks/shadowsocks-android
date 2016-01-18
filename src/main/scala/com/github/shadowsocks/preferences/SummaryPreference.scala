package com.github.shadowsocks.preferences

import java.util.Locale

import android.preference.Preference

/**
  * Make your preference support %s in summary. Override getSummaryValue to customize what to put in.
  * @author Mygod
  */
trait SummaryPreference extends Preference {
  protected def getSummaryValue: AnyRef

  /**
    * Returns the summary of this SummaryPreference. If the summary has a String formatting marker in it
    * (i.e. "%s" or "%1$s"), then the current entry value will be substituted in its place.
    *
    * @return the summary with appropriate string substitution
    */
  override def getSummary = super.getSummary.toString.formatLocal(Locale.ENGLISH, getSummaryValue)
}
