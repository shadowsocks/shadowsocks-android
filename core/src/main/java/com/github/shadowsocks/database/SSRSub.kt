package com.github.shadowsocks.database

import android.content.Context
import androidx.room.*
import com.github.shadowsocks.core.R

@Entity
class SSRSub(
        @PrimaryKey(autoGenerate = true)
        var id: Long = 0,
        var url: String,
        var url_group: String,
        var status: Long = NORMAL
) {
    companion object {
        const val NORMAL: Long = 0
        const val EMPTY: Long = 1
        const val NETWORK_ERROR: Long = 2
        const val NAME_CHANGED: Long = 3
    }

    @androidx.room.Dao
    interface Dao {
        @Insert
        fun create(value: SSRSub): Long

        @Update
        fun update(value: SSRSub): Int

        @Query("SELECT * FROM `SSRSub` WHERE `id` = :id")
        operator fun get(id: Long): SSRSub?

        @Query("DELETE FROM `SSRSub` WHERE `id` = :id")
        fun delete(id: Long): Int

        @Query("SELECT * FROM `SSRSub`")
        fun getAll(): List<SSRSub>
    }

    fun getStatue(context: Context) = when (status) {
        NORMAL -> ""
        EMPTY -> context.getString(R.string.status_empty)
        NETWORK_ERROR -> context.getString(R.string.status_network_error)
        NAME_CHANGED -> context.getString(R.string.status_name_changed)
        else -> throw IllegalArgumentException("status: $status")
    }
}
