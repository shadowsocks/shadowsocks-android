package com.github.shadowsocks.preferences

import android.content.Context
import android.preference.Preference
import android.view.{View, ViewGroup, LayoutInflater}
import android.util.AttributeSet
import android.widget.TextView
import com.github.shadowsocks.R

/**
  * @author madeye
  */
final class StatusPreference(context: Context, attrs: AttributeSet = null)
  extends Preference(context, attrs) {
    var txView: TextView = null
    var rxView: TextView = null
    var txRateView: TextView = null
    var rxRateView: TextView = null

    override def onCreateView(parent: ViewGroup): View = {
      val li = getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE).asInstanceOf[LayoutInflater]
      val view = li.inflate(R.layout.status_pref, parent, false)

      txView = view.findViewById(R.id.tx).asInstanceOf[TextView]
      rxView = view.findViewById(R.id.rx).asInstanceOf[TextView]
      txRateView = view.findViewById(R.id.txRate).asInstanceOf[TextView]
      rxRateView = view.findViewById(R.id.rxRate).asInstanceOf[TextView]

      view
    }

    def setRate(tx: String, rx: String, txRate: String, rxRate: String) {
      if (txView != null) {
        txView.setText(tx)
      }
      if (rxView != null) {
        rxView.setText(rx)
      }
      if (txRateView != null) {
        txRateView.setText(txRate)
      }
      if (rxRateView != null) {
        rxRateView.setText(rxRate)
      }
    }
}
