import 'package:flutter/services.dart';
import '../models/profile.dart';

class ProfileChannel {
  static const _method = MethodChannel('com.github.shadowsocks/profiles');

  Future<List<Profile>> getProfiles() async {
    final result = await _method.invokeListMethod<Map>('getProfiles');
    return result?.map((m) => Profile.fromMap(m)).toList() ?? [];
  }

  Future<Profile?> getProfile(int id) async {
    final result =
        await _method.invokeMapMethod<String, dynamic>('getProfile', {'id': id});
    return result != null ? Profile.fromMap(result) : null;
  }

  Future<Profile?> createProfile(Map<String, dynamic> data) async {
    final result =
        await _method.invokeMapMethod<String, dynamic>('createProfile', data);
    return result != null ? Profile.fromMap(result) : null;
  }

  Future<bool> updateProfile(Map<String, dynamic> data) async {
    final result = await _method.invokeMethod<bool>('updateProfile', data);
    return result ?? false;
  }

  Future<bool> deleteProfile(int id) async {
    final result =
        await _method.invokeMethod<bool>('deleteProfile', {'id': id});
    return result ?? false;
  }

  Future<void> selectProfile(int id) async {
    await _method.invokeMethod('selectProfile', {'id': id});
  }

  Future<int> getSelectedId() async {
    final result = await _method.invokeMethod<int>('getSelectedId');
    return result ?? 0;
  }

  Future<void> reorderProfiles(List<Map<String, int>> order) async {
    await _method.invokeMethod('reorderProfiles', {'order': order});
  }

  Future<int> importFromText(String text) async {
    final result =
        await _method.invokeMethod<int>('importFromText', {'text': text});
    return result ?? 0;
  }

  Future<int> importFromJson(String json) async {
    final result =
        await _method.invokeMethod<int>('importFromJson', {'json': json});
    return result ?? 0;
  }

  Future<String> exportToJson() async {
    final result = await _method.invokeMethod<String>('exportToJson');
    return result ?? '[]';
  }

  Future<String?> getProfileUri(int id) async {
    return await _method.invokeMethod<String>('getProfileUri', {'id': id});
  }

  Future<List<Map>> getPlugins() async {
    final result = await _method.invokeListMethod<Map>('getPlugins');
    return result ?? [];
  }
}
