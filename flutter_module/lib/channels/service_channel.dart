import 'package:flutter/services.dart';
import '../models/service_state.dart';

class ServiceChannel {
  static const _method = MethodChannel('com.github.shadowsocks/service');
  static const _stateEvent = EventChannel('com.github.shadowsocks/state');

  Future<ServiceState> getState() async {
    final state = await _method.invokeMethod<int>('getState');
    return ServiceState.fromValue(state ?? 0);
  }

  Future<bool> toggle() async {
    final result = await _method.invokeMethod<bool>('toggle');
    return result ?? false;
  }

  Future<bool> requestVpnPermission() async {
    final result = await _method.invokeMethod<bool>('requestVpnPermission');
    return result ?? false;
  }

  Future<String?> testConnection() async {
    return await _method.invokeMethod<String>('testConnection');
  }

  Stream<ServiceStatus> get stateStream {
    return _stateEvent.receiveBroadcastStream().map((event) {
      return ServiceStatus.fromMap(event as Map<dynamic, dynamic>);
    });
  }
}
