package com.github.shadowsocks.preferences

import android.content.Context
import android.content.res.TypedArray
import android.os.Bundle
import android.preference.DialogPreference
import android.util.AttributeSet
import android.view.{ViewGroup, WindowManager}
import android.widget.NumberPicker
import com.github.shadowsocks.R

/**
  * @author Mygod
  */
final class NumberPickerPreference(context: Context, attrs: AttributeSet = null)
  extends DialogPreference(context, attrs) with SummaryPreference {
  private val picker = new NumberPicker(context)
  private var value: Int = _

  {
    val a: TypedArray = context.obtainStyledAttributes(attrs, R.styleable.NumberPickerPreference)
    setMin(a.getInt(R.styleable.NumberPickerPreference_min, 0))
    setMax(a.getInt(R.styleable.NumberPickerPreference_max, Int.MaxValue - 1))
    a.recycle
  }

  def getValue = value
  def getMin = if (picker == null) 0 else picker.getMinValue
  def getMax = picker.getMaxValue
  def setValue(i: Int) {
    if (i == getValue || !callChangeListener(i)) return
    picker.setValue(i)
    value = picker.getValue
    persistInt(value)
    notifyChanged
  }
  def setMin(value: Int) = picker.setMinValue(value)
  def setMax(value: Int) = picker.setMaxValue(value)

  override protected def showDialog(state: Bundle) {
    super.showDialog(state)
    getDialog.getWindow.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE)
  }
  override protected def onCreateDialogView = {
    val parent = picker.getParent.asInstanceOf[ViewGroup]
    if (parent != null) parent.removeView(picker)
    picker
  }
  override protected def onDialogClosed(positiveResult: Boolean) {
    picker.clearFocus // commit changes
    super.onDialogClosed(positiveResult)  // forward compatibility
    if (positiveResult) setValue(picker.getValue) else picker.setValue(value)
  }
  override protected def onGetDefaultValue(a: TypedArray, index: Int) = a.getInt(index, getMin).asInstanceOf[AnyRef]
  override protected def onSetInitialValue(restorePersistedValue: Boolean, defaultValue: Any) {
    val default = defaultValue.asInstanceOf[Int]
    setValue(if (restorePersistedValue) getPersistedInt(default) else default)
  }
  protected def getSummaryValue: AnyRef = getValue.asInstanceOf[AnyRef]
}
