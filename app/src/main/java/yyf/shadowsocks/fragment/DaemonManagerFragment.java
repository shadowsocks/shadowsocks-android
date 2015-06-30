package yyf.shadowsocks.fragment;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import yyf.shadowsocks.R;

/**
 * Created by yyf on 2015/6/10.
 */
public class DaemonManagerFragment extends Fragment{
        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                                 Bundle savedInstanceState) {
            return inflater.inflate(R.layout.daemon_manage, container, false);
    }
}

