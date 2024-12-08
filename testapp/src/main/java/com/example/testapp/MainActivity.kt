package com.example.testapp

import android.content.res.Configuration
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceDataStore
import com.github.shadowsocks.Core
import com.github.shadowsocks.aidl.IShadowsocksService
import com.github.shadowsocks.aidl.ShadowsocksConnection
import com.github.shadowsocks.bg.BaseService
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.preference.OnPreferenceDataStoreChangeListener
import com.github.shadowsocks.utils.StartService
import timber.log.Timber

class MainActivity : AppCompatActivity(), ShadowsocksConnection.Callback,
    OnPreferenceDataStoreChangeListener {

    lateinit var btn: Button
    lateinit var btn2: Button
    lateinit var btn3: Button
    lateinit var textv: TextView

    private val connect = registerForActivityResult(StartService()) {
        if (it) Timber.tag("Profile").i("error opening service");
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        btn = findViewById(R.id.test_listall);
        btn2 = findViewById(R.id.test_start);
        btn3 = findViewById(R.id.test_stop);
        textv = findViewById(R.id.test_textview);

        btn3.setOnClickListener {
            println(Core.stopService().toString());
        }

        btn2.setOnClickListener {
            connect.launch(null)
            Core.switchProfile(ProfileManager.getAllProfiles()?.get(0)?.id ?: 0)

            Core.currentProfile?.let {
                println(it.toString())
            }
            println(Core.startService().toString());
        }

        btn.setOnClickListener {
            ProfileManager.clear()
            val profiles = Profile.findAllUrls("ss://eGNoYWNoYTIwLWlldGYtcG9seTEzMDU6QXhlN0oyYWtHIUBvQmdU@us.vpn.cocomine.cc:6373/#US_vpn")
            for (profile in profiles) {
                ProfileManager.createProfile(profile)
            }

            ProfileManager.getAllProfiles()?.forEach { profile ->
                println(profile.id.toString() + profile.toString())
            }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        Core.updateNotificationChannels()
    }

    override fun stateChanged(state: BaseService.State, profileName: String?, msg: String?) {
        println(state)
        println(profileName)
        println(msg)
    }

    override fun onServiceConnected(service: IShadowsocksService) {
        println(service)
    }

    override fun onPreferenceDataStoreChanged(store: PreferenceDataStore, key: String) {
        println(store)
        println(key)
    }
}

