package yyf.shadowsocks.service;

import android.app.Notification;
import android.content.Context;
import android.net.VpnService;
import android.os.Handler;
import android.os.RemoteCallbackList;
import android.util.Log;

import yyf.shadowsocks.IShadowsocksService;
import yyf.shadowsocks.utils.Constants;
import yyf.shadowsocks.Config;
//import yyf.shadowsocks.aidl.IShadowsocksServiceCallback;

/**
 * Created by yyf on 2015/6/18.
 */
public abstract class BaseService extends VpnService {
        volatile private Constants.State state = Constants.State.INIT;
        //volatile private int callbackCount = 0;
        //final RemoteCallbackList callbacks = new RemoteCallbackList<IShadowsocksServiceCallback>();
        IShadowsocksService.Stub binder = new IShadowsocksService.Stub(){
            public int getMode(){
                    return getServiceMode().ordinal();
            }

            public int getState(){
                    return state.ordinal();
            }

            public void stop() {
                if (state != Constants.State.CONNECTING && state != Constants.State.STOPPING) {
                    stopRunner();
                }
            }

            public void start(Config config) {
                if (state != Constants.State.CONNECTING && state != Constants.State.STOPPING) {
                    startRunner(config);
                }
            }
        };

        public abstract void stopBackgroundService();
        public abstract void startRunner(Config config);
        public abstract void stopRunner();
        public abstract Constants.Mode getServiceMode();
        public abstract String getTag();
        public abstract Context getContext();

        public int getCallbackCount(){
        //        return callbackCount;
            return -1;
        }
        public Constants.State getState(){
                return state;
        }
        public void changeState(Constants.State s) {
            changeState(s, null);
        }

        protected void changeState(final Constants.State s,final String msg) {
//            Handler handler = new Handler(getContext().getMainLooper());
//            handler.post(new Runnable() {
//                public void run() {
//                    if (state != s) {
//                        if (callbackCount > 0) {
//                            int n = callbacks.beginBroadcast();
//                            for (int i = 0; i < n; i++) {
//                                //callbacks.getBroadcastItem(i).stateChanged(s.ordinal(),msg);
//                                Log.v("ss-error", "badbad");
//                            }
//                            callbacks.finishBroadcast();
//                        }
//                        state = s;
//                    }
//                }
//            });

        }

        void initSoundVibrateLights(Notification notification) {
        notification.sound = null;
    }

}
