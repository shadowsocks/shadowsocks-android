package yyf.shadowsocks.utils;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.BaseColumns;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

import yyf.shadowsocks.preferences.ServerPreference;

public class ServerListDB extends SQLiteOpenHelper{
    private static final String TEXT_TYPE = " TEXT";
    private static final String INT_TYPE =" INTEGER";
    private static final String COMMA_SEP = ",";
    private static final String SQL_CREATE_ENTRIES =
            "CREATE TABLE " + ServerColums.TABLE_NAME + " (" +
                    ServerColums._ID + " INTEGER PRIMARY KEY," +
                    ServerColums.COLUMN_NAME            + TEXT_TYPE     + COMMA_SEP +
                    ServerColums.COLUMN_HOST            + TEXT_TYPE     +COMMA_SEP +
                    ServerColums.COLUMN_REMOTE_PORT    + INT_TYPE      + COMMA_SEP +
                    ServerColums.COLUMN_METHOD          + TEXT_TYPE     + COMMA_SEP +
                    ServerColums.COLUMN_PASSWORD        + TEXT_TYPE     + COMMA_SEP +
                    ServerColums.COLUMN_LOCAL_PORT      + INT_TYPE      +
            " )";
    private static final String SQL_DELETE_ENTRIES =
            "DROP TABLE IF EXISTS " + ServerColums.TABLE_NAME;
    public static final int DATABASE_VERSION = 1;
    public static final String DATABASE_NAME = "profile.db";
    public ServerListDB(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
    }
    @Override
    public void onCreate(SQLiteDatabase db) {
        Log.v("ss-android", "SQLite Create");
        db.execSQL(SQL_CREATE_ENTRIES);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {

    }
    public boolean add(ServerPreference sp){
        return true;
    }
    public boolean add(List<ServerPreference> list){
        Log.v("ss-android", "AddDataBase");
        SQLiteDatabase db = getWritableDatabase();
        for(int i = 0 ; i < list.size() ; i++) {
            ContentValues cv = new ContentValues();
            cv.put(ServerColums.COLUMN_HOST,list.get(i).getRemarks());
            cv.put(ServerColums.COLUMN_LOCAL_PORT,list.get(i).getLocal_port());
            cv.put(ServerColums.COLUMN_METHOD,list.get(i).getMethod());
            cv.put(ServerColums.COLUMN_NAME,list.get(i).getServer());
            cv.put(ServerColums.COLUMN_PASSWORD,list.get(i).getPassword());
            cv.put(ServerColums.COLUMN_REMOTE_PORT,list.get(i).getServer_port());
            if(-1 == db.insert(ServerColums.TABLE_NAME, null,cv ))
                return false;
        }
        return true;
    }
    public boolean deleteFromName(String name){
        return true;
    }
    public boolean deleteAll(){
        Log.v("ss-android", "DeleteDataBase");
        SQLiteDatabase db = getWritableDatabase();
        db.delete(ServerColums.TABLE_NAME,null,null);
        return true;
    }
    public List<ServerPreference> get(){
        Log.v("ss-android","GetDataBase");
        SQLiteDatabase db = getWritableDatabase();
        String sql = "select * from profile;";
        Cursor cursor = db.rawQuery(sql, null);
        List<ServerPreference> list = new ArrayList<>();
        while(cursor.moveToNext()) {
            ServerPreference sp = new ServerPreference();
            sp.setServer(cursor.getString(cursor.getColumnIndex(ServerColums.COLUMN_HOST)));
            sp.setServer_port(cursor.getInt(cursor.getColumnIndex(ServerColums.COLUMN_REMOTE_PORT)));
            sp.setMethod(cursor.getString(cursor.getColumnIndex(ServerColums.COLUMN_METHOD)));
            sp.setPassword(cursor.getString(cursor.getColumnIndex(ServerColums.COLUMN_PASSWORD)));
            sp.setLocal_port(cursor.getInt(cursor.getColumnIndex(ServerColums.COLUMN_LOCAL_PORT)));
            sp.setRemarks(cursor.getString(cursor.getColumnIndex(ServerColums.COLUMN_NAME)));
            list.add(sp);
            //Log.v("ss-android", sp.toString());
            cursor.moveToNext();
            //Log.v("ss-android",sp.toString());
        }
        return list;
    }

    public static abstract class ServerColums implements BaseColumns {
        public static final String TABLE_NAME = "profile";
        public static final String COLUMN_NAME = "name";
        public static final String COLUMN_HOST = "host";
        public static final String COLUMN_PASSWORD = "password";
        public static final String COLUMN_REMOTE_PORT = "remote_port";
        public static final String COLUMN_METHOD = "method";
        public static final String COLUMN_LOCAL_PORT = "local_port";
    }
}
