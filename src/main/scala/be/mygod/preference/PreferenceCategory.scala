package be.mygod.preference

import android.content.Context
import android.support.v7.preference.{PreferenceViewHolder, PreferenceCategory => Base}
import android.util.AttributeSet
import android.view.ViewGroup.MarginLayoutParams

/**
  * Based on: https://github.com/Gericop/Android-Support-Preference-V7-Fix/blob/4c2bb2896895dbedb37b659b36bd2a96b33c1605/preference-v7/src/main/java/com/takisoft/fix/support/v7/preference/PreferenceCategory.java
  *
  * @author Mygod
  */
class PreferenceCategory(context: Context, attrs: AttributeSet = null) extends Base(context, attrs) {
  override def onBindViewHolder(holder: PreferenceViewHolder) {
    super.onBindViewHolder(holder)
    holder.findViewById(android.R.id.title).getLayoutParams.asInstanceOf[MarginLayoutParams].bottomMargin = 0
  }
}
