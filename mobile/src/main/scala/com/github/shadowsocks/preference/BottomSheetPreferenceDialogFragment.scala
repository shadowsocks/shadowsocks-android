package com.github.shadowsocks.preference

import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.support.design.widget.BottomSheetDialog
import android.support.v14.preference.PreferenceDialogFragment
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.{LinearLayoutManager, RecyclerView}
import android.view.ViewGroup.LayoutParams
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.TextView
import com.github.shadowsocks.R

/**
  * @author Mygod
  */
final class BottomSheetPreferenceDialogFragment extends PreferenceDialogFragment {
  private lazy val preference = getPreference.asInstanceOf[IconListPreference]
  private lazy val index: Int = preference.selectedEntry
  private lazy val entries: Array[CharSequence] = preference.getEntries
  private lazy val entryIcons: Array[Drawable] = preference.getEntryIcons
  private var clickedIndex = -1

  override def onCreateDialog(savedInstanceState: Bundle): BottomSheetDialog = {
    val activity = getActivity
    val dialog = new BottomSheetDialog(activity, getTheme)
    val recycler = new RecyclerView(activity)
    recycler.setHasFixedSize(true)
    recycler.setLayoutManager(new LinearLayoutManager(activity))
    recycler.setAdapter(new IconListAdapter(dialog))
    recycler.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    dialog.setContentView(recycler)
    dialog
  }

  def onDialogClosed(positiveResult: Boolean): Unit = if (clickedIndex >= 0) {
    val value = preference.getEntryValues()(clickedIndex).toString
    if (preference.callChangeListener(value)) preference.setValue(value)
  }

  private final class IconListViewHolder(val dialog: BottomSheetDialog, val view: TextView) extends ViewHolder(view)
    with View.OnClickListener {
    private var index: Int = _

    {
      val padding = getResources.getDimension(R.dimen.icon_list_item_padding).toInt
      view.setPadding(padding, padding, padding, padding)
      view.setCompoundDrawablePadding(padding)
      view.setOnClickListener(this)
      val typedArray = dialog.getContext.obtainStyledAttributes(Array(android.R.attr.selectableItemBackground))
      view.setBackgroundResource(typedArray.getResourceId(0, 0))
      typedArray.recycle()
    }

    def bind(i: Int, selected: Boolean = false) {
      view.setText(entries(i))
      view.setCompoundDrawablesWithIntrinsicBounds(entryIcons(i), null, null, null)
      view.setTypeface(null, if (selected) Typeface.BOLD else Typeface.NORMAL)
      index = i
    }
    def onClick(v: View) {
      clickedIndex = index
      dialog.dismiss()
    }
  }
  private final class IconListAdapter(dialog: BottomSheetDialog) extends RecyclerView.Adapter[IconListViewHolder] {
    def getItemCount: Int = entries.length
    def onBindViewHolder(vh: IconListViewHolder, i: Int): Unit = if (index < 0) vh.bind(i) else i match {
      case 0 => vh.bind(index, selected = true)
      case _ if i > index => vh.bind(i)
      case _ => vh.bind(i - 1)
    }
    def onCreateViewHolder(vg: ViewGroup, i: Int): IconListViewHolder = new IconListViewHolder(dialog,
      LayoutInflater.from(vg.getContext).inflate(android.R.layout.simple_list_item_1, vg, false).asInstanceOf[TextView])
  }
}
