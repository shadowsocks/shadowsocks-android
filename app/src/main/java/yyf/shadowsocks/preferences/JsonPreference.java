package yyf.shadowsocks.preferences;

import android.os.Environment;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by yyf on 2015/6/11.
 */
public class JsonPreference {

    public static List<ServerPreference> getServerPerferenc(){
        try {
            String str = getFileString();
            JSONObject jsonObject =new JSONObject(str);
            JSONArray jsonArray = (JSONArray)jsonObject.get("configs");
            Log.v("ss=android",jsonObject.get("configs").toString());
            List<ServerPreference> list = new ArrayList<ServerPreference>();
            for(int i = 0;i<jsonArray.length();i++){
                ServerPreference sp = new ServerPreference();
                JSONObject jo = jsonArray.getJSONObject(i);
                sp.setServer((String)jo.get("server"));
                sp.setLocal_port(jo.getInt("local_port"));
                sp.setMethod((String) jo.get("method"));
                sp.setPassword((String) jo.get("password"));
                sp.setRemarks((String)jo.get("remarks"));
                sp.setServer_port(jo.getInt("server_port"));
                list.add(sp);
            }
            return list;
        } catch (JSONException e) {
            e.printStackTrace();
            return null;
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }
    private static String getFileString() throws IOException {
        File file = new File(Environment.getExternalStorageDirectory(),"ss-config.json");
        FileInputStream fs = null;
        InputStreamReader isr = null;
        BufferedReader br = null;
        String str = "";
        try {
            fs = new FileInputStream(file);
            isr = new InputStreamReader(fs,"UTF-8");
            br = new BufferedReader(isr);
            String temp=null;
            while((temp = br.readLine()) != null)
                str+=temp;
            return str;
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return str;
        } catch (UnsupportedEncodingException e){
            e.printStackTrace();
            return str;
        }finally {
            br.close();
            isr.close();
            fs.close();
            return str;
        }
    }
}
