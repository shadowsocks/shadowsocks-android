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

  /// Launches the plugin's native configuration activity.
  /// Returns {'status': 'ok', 'options': '...'}, {'status': 'fallback'}, or {'status': 'cancelled'}.
  Future<Map<String, dynamic>> configurePlugin(String pluginId, String options) async {
    final result = await _method.invokeMapMethod<String, dynamic>(
      'configurePlugin',
      {'pluginId': pluginId, 'options': options},
    );
    return result ?? {'status': 'fallback'};
  }

  Stream<ServiceStatus> get stateStream {
    return _stateEvent.receiveBroadcastStream().map((event) {
      return ServiceStatus.fromMap(event as Map<dynamic, dynamic>);
    });
  }
}
