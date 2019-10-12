package com.github.shadowsocks.database

import androidx.room.*

@Entity
class SSRSub(
        @PrimaryKey(autoGenerate = true)
        var id: Long = 0,
        var url: String,
        var url_group: String
) {
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
}