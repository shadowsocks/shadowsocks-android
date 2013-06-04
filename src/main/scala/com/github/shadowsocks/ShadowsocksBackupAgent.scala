package com.github.shadowsocks

import android.app.backup.{SharedPreferencesBackupHelper, BackupAgentHelper}

class ShadowsocksBackupAgent extends BackupAgentHelper {

  // The names of the SharedPreferences groups that the application maintains.  These
  // are the same strings that are passed to getSharedPreferences(String, int).
  val PREFS_DISPLAY = "com.github.shadowsocks_preferences"

  // An arbitrary string used within the BackupAgentHelper implementation to
  // identify the SharedPreferencesBackupHelper's data.
  val MY_PREFS_BACKUP_KEY = "com.github.shadowsocks"

  override def onCreate() {
    val helper = new SharedPreferencesBackupHelper(this, PREFS_DISPLAY)
    addHelper(MY_PREFS_BACKUP_KEY, helper)
  }
}
