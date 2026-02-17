import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../channels/profile_channel.dart';
import '../models/profile.dart';

final profileChannelProvider = Provider((ref) => ProfileChannel());

final profilesProvider =
    AsyncNotifierProvider<ProfilesNotifier, List<Profile>>(
        ProfilesNotifier.new);

class ProfilesNotifier extends AsyncNotifier<List<Profile>> {
  ProfileChannel get _channel => ref.read(profileChannelProvider);

  @override
  Future<List<Profile>> build() => _channel.getProfiles();

  Future<void> refresh() async {
    state = const AsyncLoading();
    state = await AsyncValue.guard(() => _channel.getProfiles());
  }

  Future<void> selectProfile(int id) async {
    await ref.read(selectedProfileIdProvider.notifier).select(id);
    state = await AsyncValue.guard(() => _channel.getProfiles());
  }

  Future<void> deleteProfile(int id) async {
    await _channel.deleteProfile(id);
    state = await AsyncValue.guard(() => _channel.getProfiles());
  }

  Future<Profile?> createProfile(Map<String, dynamic> data) async {
    final profile = await _channel.createProfile(data);
    state = await AsyncValue.guard(() => _channel.getProfiles());
    return profile;
  }

  Future<void> updateProfile(Map<String, dynamic> data) async {
    await _channel.updateProfile(data);
    state = await AsyncValue.guard(() => _channel.getProfiles());
  }

  Future<int> importFromText(String text) async {
    final count = await _channel.importFromText(text);
    if (count > 0) {
      state = await AsyncValue.guard(() => _channel.getProfiles());
    }
    return count;
  }

  Future<int> importFromJson(String json) async {
    final count = await _channel.importFromJson(json);
    state = await AsyncValue.guard(() => _channel.getProfiles());
    return count;
  }

  Future<String> exportToJson() => _channel.exportToJson();
}

final selectedProfileIdProvider =
    AsyncNotifierProvider<SelectedProfileNotifier, int>(
        SelectedProfileNotifier.new);

class SelectedProfileNotifier extends AsyncNotifier<int> {
  @override
  Future<int> build() async {
    final channel = ref.read(profileChannelProvider);
    return channel.getSelectedId();
  }

  Future<void> select(int id) async {
    final channel = ref.read(profileChannelProvider);
    await channel.selectProfile(id);
    state = AsyncData(id);
  }
}
