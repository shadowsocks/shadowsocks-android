package com.github.shadowsocks.preferences

import android.annotation.TargetApi
import android.content.Context
import android.content.res.TypedArray
import android.os.Build
import android.preference.Preference
import android.support.annotation.NonNull
import android.support.v7.widget.AppCompatSpinner
import android.util.AttributeSet
import android.view.{View, ViewGroup}
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.{AdapterView, ArrayAdapter}
import com.github.shadowsocks.R

/**
  * @author Mygod
  */
final class DropDownPreference(private val mContext: Context, attrs: AttributeSet = null)
  extends Preference(mContext, attrs) with SummaryPreference {

  private val mAdapter = new ArrayAdapter[String](mContext, android.R.layout.simple_spinner_dropdown_item)
  private val mSpinner = new AppCompatSpinner(mContext)
  private var mEntries: Array[CharSequence] = _
  private var mEntryValues: Array[CharSequence] = _
  private var mSelectedIndex: Int = _
  private var mValueSet: Boolean = _

  mSpinner.setVisibility(View.INVISIBLE)
  mSpinner.setAdapter(mAdapter)
  mSpinner.setOnItemSelectedListener(new OnItemSelectedListener {
    override def onNothingSelected(parent: AdapterView[_]) { }
    override def onItemSelected(parent: AdapterView[_], view: View, position: Int, id: Long) = setValueIndex(position)
  })
  setOnPreferenceClickListener((preference: Preference) => {
    // TODO: not working with scrolling
    // mSpinner.setDropDownVerticalOffset(Utils.dpToPx(getContext, -48 * mSelectedIndex).toInt)
    mSpinner.performClick
    true
  })
  val a: TypedArray = mContext.obtainStyledAttributes(attrs, R.styleable.DropDownPreference)
  setEntries(a.getTextArray(R.styleable.DropDownPreference_entries))
  mEntryValues = a.getTextArray(R.styleable.DropDownPreference_entryValues)
  a.recycle

  protected override def getSummaryValue = getEntry

  /**
    * Sets the human-readable entries to be shown in the list. This will be shown in subsequent dialogs.
    *
    * Each entry must have a corresponding index in [[setEntryValues(CharSequence[])]].
    *
    * @param entries The entries.
    * @see [[setEntryValues(CharSequence[])]]
    */
  def setEntries(entries: Array[CharSequence]) {
    mEntries = entries
    mAdapter.clear
    if (entries != null) for (entry <- entries) mAdapter.add(entry.toString)
  }
  /**
    * @see #setEntries(CharSequence[])
    * @param entriesResId The entries array as a resource.
    */
  def setEntries(entriesResId: Int): Unit = setEntries(getContext.getResources.getTextArray(entriesResId))
  /**
    * The list of entries to be shown in the list in subsequent dialogs.
    *
    * @return The list as an array.
    */
  def getEntries = mEntries

  /**
    * The array to find the value to save for a preference when an entry from
    * entries is selected. If a user clicks on the second item in entries, the
    * second item in this array will be saved to the preference.
    *
    * @param entryValues The array to be used as values to save for the preference.
    */
  def setEntryValues(entryValues: Array[CharSequence]) = mEntryValues = entryValues
  /**
    * @see #setEntryValues(CharSequence[])
    * @param entryValuesResId The entry values array as a resource.
    */
  def setEntryValues(entryValuesResId: Int): Unit =
    setEntryValues(getContext.getResources.getTextArray(entryValuesResId))
  /**
    * Returns the array of values to be saved for the preference.
    *
    * @return The array of values.
    */
  def getEntryValues: Array[CharSequence] = mEntryValues

  /**
    * Sets the value of the key. This should be one of the entries in [[getEntryValues]].
    *
    * @param value The value to set for the key.
    */
  def setValue(value: String) {
    val i = findIndexOfValue(value)
    if (i > -1) setValueIndex(i)
  }

  /**
    * Sets the value to the given index from the entry values.
    *
    * @param index The index of the value to set.
    */
  def setValueIndex(index: Int) {
    val changed = mSelectedIndex != index
    if (changed || !mValueSet) {
      mSelectedIndex = index
      mValueSet = true
      if (mEntryValues != null) {
        val value = mEntryValues(index).toString
        persistString(value)
        setValue(value)
      }
      if (changed) notifyChanged
    }
    mSpinner.setSelection(index)
  }

  /**
    * Returns the value of the key. This should be one of the entries in [[getEntryValues]].
    *
    * @return The value of the key.
    */
  def getValue = if (mEntryValues == null) null else mEntryValues(mSelectedIndex).toString

  /**
    * Returns the entry corresponding to the current value.
    *
    * @return The entry corresponding to the current value, or null.
    */
  def getEntry = {
    val index = getValueIndex
    if (index >= 0 && mEntries != null) mEntries(index) else null
  }

  /**
    * Returns the index of the given value (in the entry values array).
    *
    * @param value The value whose index should be returned.
    * @return The index of the value, or -1 if not found.
    */
  def findIndexOfValue(value: String): Int = {
    if (value != null && mEntryValues != null) {
      var i = mEntryValues.length - 1
      while (i >= 0) {
        if (mEntryValues(i) == value) return i
        i -= 1
      }
    }
    -1
  }

  def getValueIndex = mSelectedIndex

  protected override def onGetDefaultValue(a: TypedArray, index: Int) = a.getString(index)
  protected override def onSetInitialValue (restoreValue: Boolean, defaultValue: AnyRef) =
    setValue(if (restoreValue) getPersistedString(getValue) else defaultValue.asInstanceOf[String])

  @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
  def setDropDownWidth(dimenResId: Int) =
    mSpinner.setDropDownWidth(mContext.getResources.getDimensionPixelSize(dimenResId))

  protected override def onBindView(@NonNull view: View) {
    super.onBindView(view)
    val parent = mSpinner.getParent.asInstanceOf[ViewGroup]
    if (view eq parent) return
    if (parent != null) parent.removeView(mSpinner)
    view.asInstanceOf[ViewGroup].addView(mSpinner, 0)
    val lp = mSpinner.getLayoutParams
    lp.width = 0
    mSpinner.setLayoutParams(lp)
  }
}
