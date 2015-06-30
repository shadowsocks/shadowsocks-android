package yyf.shadowsocks;

//import yyf.shadowsocks.aidl.IShadowsocksServiceCallback;
import yyf.shadowsocks.Config;
interface IShadowsocksService {
  int getMode();
  int getState();

  //oneway void registerCallback(IShadowsocksServiceCallback cb);
  //oneway void unregisterCallback(IShadowsocksServiceCallback cb);

  oneway void start(in Config config);
  oneway void stop();
}
