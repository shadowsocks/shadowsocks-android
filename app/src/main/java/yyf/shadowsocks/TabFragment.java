package yyf.shadowsocks;


import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import java.util.ArrayList;
import java.util.List;

import yyf.shadowsocks.utils.ServerListDB;
import yyf.shadowsocks.preferences.ServerPreference;

/**
 * Created by yyf on 2015/6/10.
 */
public  class TabFragment extends Fragment {
    /**
     * The fragment argument representing the section number for this
     * fragment.
     */
    private static final String ARG_SECTION_NUMBER = "section_number";

    /**
     * Returns a new instance of this fragment for the given section
     * number.
     */
    public static Fragment newInstance(int sectionNumber) {
        Bundle args = new Bundle();
        Fragment fa = new TabFragment();
        args.putInt(ARG_SECTION_NUMBER, sectionNumber);
        fa.setArguments(args);
        Log.v("ss-android","new Fragment"+sectionNumber);
        return fa;
    }
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        int pageNo = getArguments().getInt(ARG_SECTION_NUMBER);
        View rootView;
        //rootView = inflater.inflate(R.layout.log_cat, container, false);
        switch(pageNo){
            case 1:
                return initServerList(inflater,container,savedInstanceState);
            case 2:
                Log.v("ss-android",pageNo+"hehe");
                rootView = inflater.inflate(R.layout.daemon_manage, container, false);
                break;
            case 3:
                Log.v("ss-android",pageNo+"hehe");
                rootView = inflater.inflate(R.layout.log_cat, container, false);
                break;
            default:
                Log.v("ss-android",pageNo+"hehe");
                rootView = inflater.inflate(R.layout.server_list, container, false);
        }
        return rootView;
    }
    View initServerList(LayoutInflater inflater, ViewGroup container,Bundle savedInstanceState){
        Log.v("ss-android", 1 + "hehe");
        /*ArrayAdapter<View> adapter = new ArrayAdapter(getActivity(), android.R.layout.simple_list_item_1);
        ListView listView = (ListView)inflater.inflate(R.layout.server_list, container, false);
        listView.setAdapter(adapter);
        //listView.findViewById()
        View item = inflater.inflate(R.layout.server_item, container, false);
        TextView text = (TextView)item.findViewById(R.id.server_name);
        text.append("hehe");
        Log.v("ss-android",text.getText().toString());
        adapter.add(item);
        return listView;*/
        ServerListDB sldb = new ServerListDB(getActivity());
        List<ServerPreference> serverList = sldb.get();
        Log.v("ss-log", "getDB");
        ArrayList<String> list = new ArrayList<>();
        for(int i = 0;i<serverList.size();i++){
            list.add(serverList.get(i).getRemarks());
            //Log.v("ss-log","add server" + serverList.get(i).getRemarks());
        }

        ServerListAdapter adapter = new ServerListAdapter(getActivity(),list);
        ListView listView = (ListView)inflater.inflate(R.layout.server_list, container, false);
        listView.setAdapter(adapter);
        return listView;
        /*ListView listView = (ListView)inflater.inflate(R.layout.server_list, container, false);
        ArrayList<HashMap<String,Object>> list = new ArrayList<>();
        HashMap map1 = new HashMap();
        map1.put("name",listView.findViewById(R.id.server_name));
        map1.put("button", listView.findViewById(R.id.server_edit));
        list.add(map1);
        HashMap map2 = new HashMap();
        map2.put("name",listView.findViewById(R.id.server_name));
        map2.put("button", listView.findViewById(R.id.server_edit));
        list.add(map2);
        SimpleAdapter adapter = new SimpleAdapter(this.getActivity(),list,R.layout.server_item,new String[] { "name", "button"},
                new int[] { R.id.server_name, R.id.server_edit});
        listView.setAdapter(adapter);
        return listView;*/

    }
}

