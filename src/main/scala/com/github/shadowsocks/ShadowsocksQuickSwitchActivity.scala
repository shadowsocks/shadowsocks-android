package com.github.shadowsocks

import android.content.res.Resources
import android.os.{Handler, Build, Bundle}
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.{Toolbar, DefaultItemAnimator, LinearLayoutManager, RecyclerView}
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.CheckedTextView
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.utils.Utils

/**
  * Created by Lucas on 3/10/16.
  */
class ShadowsocksQuickSwitchActivity extends AppCompatActivity {

  private class ProfileViewHolder(val view: View) extends RecyclerView.ViewHolder(view) with View.OnClickListener {
    {
      val typedArray = obtainStyledAttributes(Array(android.R.attr.selectableItemBackground))
      view.setBackgroundResource(typedArray.getResourceId(0, 0))
      typedArray.recycle
    }
    private var item: Profile = _
    private val text = itemView.findViewById(android.R.id.text1).asInstanceOf[CheckedTextView]
    itemView.setOnClickListener(this)

    def bind(item: Profile) {
      this.item = item
      text.setText(item.name)
      if (item.id == ShadowsocksApplication.profileId) {
        text.setChecked(true)
      } else {
        text.setChecked(false)
      }
    }

    def onClick(v: View) {
      Utils.stopSsService(ShadowsocksQuickSwitchActivity.this)
      ShadowsocksApplication.switchProfile(item.id)
      val handler = new Handler(ShadowsocksQuickSwitchActivity.this.getMainLooper)
      handler.postDelayed(() => {
        Utils.startSsService(ShadowsocksQuickSwitchActivity.this)
      }, 3000)
      finish
    }
  }

  private class ProfilesAdapter extends RecyclerView.Adapter[ProfileViewHolder] {
    val profiles = ShadowsocksApplication.profileManager.getAllProfiles.getOrElse(List.empty[Profile])

    def getItemCount = profiles.length

    def onBindViewHolder(vh: ProfileViewHolder, i: Int) = i match {
      case _ => vh.bind(profiles(i))
    }

    private val name = "select_dialog_singlechoice_" + (if (Build.VERSION.SDK_INT >= 21) "material" else "holo")

    def onCreateViewHolder(vg: ViewGroup, i: Int) = new ProfileViewHolder(LayoutInflater.from(vg.getContext)
      .inflate(Resources.getSystem.getIdentifier(name, "layout", "android"), vg, false))
  }

  private val profilesAdapter = new ProfilesAdapter

  override def onCreate(savedInstanceState: Bundle) {
    super.onCreate(savedInstanceState)
    setContentView(R.layout.layout_quick_switch)

    val toolbar = findViewById(R.id.toolbar).asInstanceOf[Toolbar]
    toolbar.setTitle(R.string.quick_switch)

    val profilesList = findViewById(R.id.profilesList).asInstanceOf[RecyclerView]
    val lm = new LinearLayoutManager(this)
    profilesList.setLayoutManager(lm)
    profilesList.setItemAnimator(new DefaultItemAnimator)
    profilesList.setAdapter(profilesAdapter)
  }
}
