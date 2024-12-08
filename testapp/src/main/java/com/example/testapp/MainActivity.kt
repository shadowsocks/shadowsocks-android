package com.example.testapp

import android.content.res.Configuration
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.google.firebase.FirebaseApp

class MainActivity : AppCompatActivity() {

    lateinit var btn: Button
    lateinit var textv: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState);

        btn = findViewById(R.id.test_listall);
        textv = findViewById(R.id.test_textview);
        btn.setOnClickListener { view ->
            // Handle the camera action
            ProfileManager.getAllProfiles()?.forEach { profile ->
                textv.append(profile.toString() + "\n")
            }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        Core.updateNotificationChannels()
    }
}

