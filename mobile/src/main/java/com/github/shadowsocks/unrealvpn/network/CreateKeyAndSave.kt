package com.github.shadowsocks.unrealvpn.network

import android.annotation.SuppressLint
import android.content.Context
import com.github.shadowsocks.Core
import com.github.shadowsocks.database.Profile
import com.github.shadowsocks.database.ProfileManager
import com.github.shadowsocks.unrealvpn.UnrealVpnStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import timber.log.Timber
import java.text.SimpleDateFormat


class CreateKeyAndSave {

    private val unrealRestService = UnrealRestService()
    suspend operator fun invoke(context: Context) {
        if (UnrealVpnStore.getAccessUrl(context) == null) {
            withContext(Dispatchers.IO) {
                val keyResponse = unrealRestService.getKey()
                Timber.d("Create response: $keyResponse")
                persistKeyResponse(context, keyResponse)

                val keyName = createKeyName()
                Timber.d("Key name: $keyName")

                unrealRestService.registerKey(keyName = keyName, keyId = keyResponse.keyId)
                unrealRestService.setLimits(keyId = keyResponse.keyId)

                initShadowSocksProfile(keyResponse.accessUrl, keyName)
            }
        }
        Timber.d("Current profile id: ${Core.currentProfile?.main?.id}")
        Timber.d("Current profile url: ${Core.currentProfile?.main?.name}")
    }

    private fun persistKeyResponse(context: Context, keyResponse: AccessKeyResponse) {
        UnrealVpnStore.setAccessUrl(context, keyResponse.accessUrl)
        UnrealVpnStore.setId(context, keyResponse.keyId)
    }

    private fun initShadowSocksProfile(accessUrl: String, keyName: String) {
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

    @SuppressLint("SimpleDateFormat")
    private fun createKeyName(): String {
        val formatter = SimpleDateFormat("yyyy-MM-dd")
        val date = formatter.format(System.currentTimeMillis())
        return "Created_$date"
    }
}
