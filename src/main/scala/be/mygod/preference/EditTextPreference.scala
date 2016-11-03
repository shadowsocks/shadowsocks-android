package be.mygod.preference

import android.content.Context
import android.support.v7.preference.{EditTextPreference => Parent}
import android.support.v7.widget.AppCompatEditText
import android.text.InputType
import android.util.AttributeSet

/**
  * Fixed EditTextPreference + SummaryPreference with password support!
  * Based on: https://github.com/Gericop/Android-Support-Preference-V7-Fix/tree/master/app/src/main/java/android/support/v7/preference
  */
class EditTextPreference(context: Context, attrs: AttributeSet = null) extends Parent(context, attrs)
  with DialogPreferencePlus with SummaryPreference {
  val editText = new AppCompatEditText(context, attrs)
  editText.setId(android.R.id.edit)

  override def createDialog() = new EditTextPreferenceDialogFragment()

  override protected def getSummaryValue = {
    var text = getText
    if (text == null) text = ""
    val inputType = editText.getInputType
    if (inputType == (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD) ||
      inputType == (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD) ||
      inputType == (InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD))
      "\u2022" * text.length else text
  }

  override def setText(text: String) = {
    super.setText(text)
    notifyChanged
  }
}
