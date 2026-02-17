import 'package:flutter/services.dart';

class SettingsChannel {
  static const _method = MethodChannel('com.github.shadowsocks/settings');

  Future<String?> getString(String key) async {
    return await _method.invokeMethod<String>('getString', {'key': key});
  }

  Future<void> putString(String key, String value) async {
    await _method.invokeMethod('putString', {'key': key, 'value': value});
  }

  Future<bool?> getBool(String key) async {
    return await _method.invokeMethod<bool>('getBool', {'key': key});
  }

  Future<void> putBool(String key, bool value) async {
    await _method.invokeMethod('putBool', {'key': key, 'value': value});
  }

  Future<String> getServiceMode() async {
    return await _method.invokeMethod<String>('getServiceMode') ?? 'vpn';
  }

  Future<void> setServiceMode(String mode) async {
    await _method.invokeMethod('setServiceMode', {'mode': mode});
  }

  Future<int> getPortProxy() async {
    return await _method.invokeMethod<int>('getPortProxy') ?? 1080;
  }

  Future<void> setPortProxy(int port) async {
    await _method.invokeMethod('setPortProxy', {'port': port});
  }

  Future<int> getPortLocalDns() async {
    return await _method.invokeMethod<int>('getPortLocalDns') ?? 5450;
  }

  Future<void> setPortLocalDns(int port) async {
    await _method.invokeMethod('setPortLocalDns', {'port': port});
  }

  Future<int> getPortTransproxy() async {
    return await _method.invokeMethod<int>('getPortTransproxy') ?? 8200;
  }

  Future<void> setPortTransproxy(int port) async {
    await _method.invokeMethod('setPortTransproxy', {'port': port});
  }

  Future<bool> getPersistAcrossReboot() async {
    return await _method.invokeMethod<bool>('getPersistAcrossReboot') ?? false;
  }

  Future<void> setPersistAcrossReboot(bool value) async {
    await _method.invokeMethod('setPersistAcrossReboot', {'value': value});
  }

  Future<bool> getDirectBootAware() async {
    return await _method.invokeMethod<bool>('getDirectBootAware') ?? false;
  }

  Future<void> setDirectBootAware(bool value) async {
    await _method.invokeMethod('setDirectBootAware', {'value': value});
  }

  Future<String> getVersion() async {
    return await _method.invokeMethod<String>('getVersion') ?? '';
  }
}
