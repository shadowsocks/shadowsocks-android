import 'package:flutter/services.dart';
import '../models/traffic_stats.dart';

class TrafficChannel {
  static const _event = EventChannel('com.github.shadowsocks/traffic');

  Stream<TrafficStats> get trafficStream {
    return _event.receiveBroadcastStream().map((event) {
      return TrafficStats.fromMap(event as Map<dynamic, dynamic>);
    });
  }
}
