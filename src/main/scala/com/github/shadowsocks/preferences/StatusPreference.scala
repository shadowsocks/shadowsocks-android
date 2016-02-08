package com.github.shadowsocks.preferences

import android.content.Context
import android.preference.Preference
import android.util.AttributeSet
import android.view.{LayoutInflater, View, ViewGroup}
import android.widget.TextView
import com.github.shadowsocks.R

/**
  * @author madeye
  */
final class StatusPreference(context: Context, attrs: AttributeSet = null)
  extends Preference(context, attrs) {
    var tx: String = _
    var rx: String = _
    var txRate: String = _
    var rxRate: String = _
    var txView: TextView = _
    var rxView: TextView = _
    var txRateView: TextView = _
    var rxRateView: TextView = _

    override def onCreateView(parent: ViewGroup): View = {
      val view = getContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE).asInstanceOf[LayoutInflater]
        .inflate(R.layout.status_pref, parent, false)

      txView = view.findViewById(R.id.tx).asInstanceOf[TextView]
      rxView = view.findViewById(R.id.rx).asInstanceOf[TextView]
      txRateView = view.findViewById(R.id.txRate).asInstanceOf[TextView]
      rxRateView = view.findViewById(R.id.rxRate).asInstanceOf[TextView]

      view
    }

    override def onBindView(view: View) = {
      super.onBindView(view)
      txView.setText(tx)
      rxView.setText(rx)
      txRateView.setText(txRate)
      rxRateView.setText(rxRate)
    }

    def setRate(tx: String, rx: String, txRate: String, rxRate: String) {
      this.tx = tx
      if (txView != null) {
        txView.setText(tx)
      }
      this.rx = rx
      if (rxView != null) {
        rxView.setText(rx)
      }
      this.txRate = txRate
      if (txRateView != null) {
        txRateView.setText(txRate)
      }
      this.rxRate = rxRate
      if (rxRateView != null) {
        rxRateView.setText(rxRate)
      }
    }
}
