package yyf.shadowsocks;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.TextView;
import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Created by yyf on 2015/6/11.
 */
public class ServerListAdapter extends BaseAdapter {
    List<String> items;
    private LayoutInflater mInflater;
    private Context context;
    public ServerListAdapter(Context context,List<String> items){
        this.items = items;
        this.mInflater = LayoutInflater.from(context);
        this.context = context;
    }
    @Override
    public int getCount() {
        return items.size();
    }

    @Override
    public Object getItem(int position) {
        return items.get(position);
    }

    @Override
    public long getItemId(int position) {
        return 0;
    }

    @Override
    public View getView(final int position, View convertView, ViewGroup parent) {
        //View view = mInflater.inflate(R.layout.server_item, null);//根据布局文件实例化view
        //观察convertView随ListView滚动情况
        /*
        if (convertView == null) {
            convertView = mInflater.inflate(R.layout.server_list,null);
        }

        TextView title = (TextView) convertView.findViewById(R.id.title);//找某个控件
        title.setText(items.get(position));//给该控件设置数据(数据从集合类中来)
        Button button = (Button)convertView.findViewById(R.id.server_edit);
        */
        ViewHolder holder;
        //观察convertView随ListView滚动情况
        Log.v("MyListViewBase", "getView " + position + " " + convertView);
        if (convertView == null) {
            convertView = mInflater.inflate(R.layout.server_item,null);
            holder = new ViewHolder();/*得到各个控件的对象*/
            holder.title = (TextView) convertView.findViewById(R.id.server_name);
            holder.bt = (Button) convertView.findViewById(R.id.server_edit);
            convertView.setTag(holder);//绑定ViewHolder对象
        }
        else{
            holder = (ViewHolder)convertView.getTag();//取出ViewHolder对象
        }
            /*设置TextView显示的内容，即我们存放在动态数组中的数据*/
        holder.title.setText(items.get(position));

            /*为Button添加点击事件*/
        holder.bt.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.v("MyListViewBase", "你点击了按钮"+position );                                //打印Button的点击信息
            }
        });

        return convertView;
    }
    public boolean add(String serverName){
        notifyDataSetChanged();
        return true;
    }
    public final class ViewHolder{
        public TextView title;
        public Button   bt;
    }
}

