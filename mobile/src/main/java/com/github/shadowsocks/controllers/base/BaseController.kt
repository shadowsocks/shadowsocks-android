package com.github.shadowsocks.controllers.base

import android.support.v4.content.ContextCompat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.bluelinelabs.conductor.archlifecycle.LifecycleController
import com.github.shadowsocks.R

abstract class BaseController : LifecycleController() {

    abstract fun inflateView(inflater: LayoutInflater, container: ViewGroup): View

    open fun onViewBound(view: View) {
        view.setBackgroundColor(ContextCompat.getColor(activity!!, R.color.background))
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup): View {
        val view = inflateView(inflater, container)
        onViewBound(view)
        return view
    }

    open fun onTrafficUpdated(profileId: Long, txRate: Long, rxRate: Long, txTotal: Long, rxTotal: Long) { }
}
