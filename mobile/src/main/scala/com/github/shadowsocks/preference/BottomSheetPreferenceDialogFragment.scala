package com.github.shadowsocks.preference

import android.content.{ActivityNotFoundException, Intent}
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.support.design.widget.BottomSheetDialog
import android.support.v7.preference.PreferenceDialogFragmentCompat
import android.support.v7.widget.RecyclerView.ViewHolder
import android.support.v7.widget.{LinearLayoutManager, RecyclerView}
import android.view.ViewGroup.LayoutParams
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.{ImageView, TextView}
import com.github.shadowsocks.R

/**
  * @author Mygod
  */
final class BottomSheetPreferenceDialogFragment extends PreferenceDialogFragmentCompat {
  private lazy val preference = getPreference.asInstanceOf[IconListPreference]
  private lazy val index: Int = preference.selectedEntry
  private lazy val entries: Array[CharSequence] = preference.getEntries
  private lazy val entryValues: Array[CharSequence] = preference.getEntryValues
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

  def onDialogClosed(positiveResult: Boolean): Unit = if (clickedIndex >= 0 && clickedIndex != index) {
    val value = preference.getEntryValues()(clickedIndex).toString
    if (preference.callChangeListener(value)) preference.setValue(value)
  }

  private final class IconListViewHolder(val dialog: BottomSheetDialog, view: View) extends ViewHolder(view)
    with View.OnClickListener with View.OnLongClickListener {
    private var index: Int = _
    private val text1 = view.findViewById[TextView](android.R.id.text1)
    private val text2 = view.findViewById[TextView](android.R.id.text2)
    private val icon = view.findViewById[ImageView](android.R.id.icon)
    view.setOnClickListener(this)
    view.setOnLongClickListener(this)

    def bind(i: Int, selected: Boolean = false) {
      text1.setText(entries(i))
      text2.setText(entryValues(i))
      val typeface = if (selected) Typeface.BOLD else Typeface.NORMAL
      text1.setTypeface(null, typeface)
      text2.setTypeface(null, typeface)
      text2.setVisibility(if (entryValues(i).length > 0 && entries(i) != entryValues(i)) View.VISIBLE else View.GONE)
      icon.setImageDrawable(entryIcons(i))
      index = i
    }
    def onClick(v: View) {
      clickedIndex = index
      dialog.dismiss()
    }
    override def onLongClick(v: View): Boolean = preference.entryPackageNames(index) match {
      case null => false
      case pn => try {
        startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, new Uri.Builder()
          .scheme("package")
          .opaquePart(pn)
          .build()))
        true
      } catch {
        case _: ActivityNotFoundException => false
      }
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
      LayoutInflater.from(vg.getContext).inflate(R.layout.icon_list_item_2, vg, false))
  }
}
