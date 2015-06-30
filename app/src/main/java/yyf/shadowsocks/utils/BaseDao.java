package yyf.shadowsocks.utils;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;

/**
 * Created by yyf on 2015/6/11.
 */
public class BaseDao {
    String PROFILE = "profile.db";
    Context context;
    SQLiteDatabase db;
    public BaseDao(Context context) {
        SQLiteDatabase db = context.openOrCreateDatabase(PROFILE, Context.MODE_PRIVATE, null);
    }
    public void close(){
        db.close();
    }
}
