package com.github.shadowsocks.unrealvpn.network

import android.annotation.SuppressLint
import android.content.Context
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.unrealvpn.UnrealVpnStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import timber.log.Timber
import java.text.SimpleDateFormat


class CreateKeyAndSave {

    private val httpClient = HttpClient()
    suspend operator fun invoke(context: Context) {
        if (UnrealVpnStore.getAccessUrl(context) == null) {
            withContext(Dispatchers.IO) {
                val keyResponse = getKey()
                Timber.d("Create response: $keyResponse")
                val keyId = keyResponse["id"].toString()
                val accessUrl = keyResponse["accessUrl"].toString()
                UnrealVpnStore.setAccessUrl(context, accessUrl)

                val keyName = createKeyName()
                UnrealVpnStore.setId(context, keyName)
                Timber.d("Key name: $keyName")
                registerKey(keyName = keyName, id = keyId)

                val emptyProfile = Profile(
                    id = 0,
                    name = keyName,
                )
                val profile = Profile.findAllUrls(accessUrl, emptyProfile).first()
                profile.name = keyName
                ProfileManager.clear()
                val created = ProfileManager.createProfile(profile)
                Core.switchProfile(created.id)
            }
        }
        Timber.d("Current profile id: ${Core.currentProfile?.main?.id}")
        Timber.d("Current profile url: ${Core.currentProfile?.main?.name}")
    }

    private fun getKey(): JSONObject {
        return JSONObject(httpClient.post("https://osa.unrealvpn.com/api/access-keys"))
    }

    private fun registerKey(keyName: String, id: String) {
        val requestBody = JSONObject()
            .put("name", keyName)
        httpClient.put(
            "https://osa.unrealvpn.com/api/access-keys/$id/name",
            requestBody.toString()
        )
    }

    @SuppressLint("SimpleDateFormat")
    private fun createKeyName(): String {
        val formatter = SimpleDateFormat("yyyy-MM-dd")
        val date = formatter.format(System.currentTimeMillis())
        return "Created_$date"
    }
}
