import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../channels/settings_channel.dart';

final settingsChannelProvider = Provider((ref) => SettingsChannel());

class SettingsState {
  final String serviceMode;
  final int portProxy;
  final int portLocalDns;
  final int portTransproxy;
  final bool persistAcrossReboot;
  final bool directBootAware;

  const SettingsState({
    this.serviceMode = 'vpn',
    this.portProxy = 1080,
    this.portLocalDns = 5450,
    this.portTransproxy = 8200,
    this.persistAcrossReboot = false,
    this.directBootAware = false,
  });

  SettingsState copyWith({
    String? serviceMode,
    int? portProxy,
    int? portLocalDns,
    int? portTransproxy,
    bool? persistAcrossReboot,
    bool? directBootAware,
  }) {
    return SettingsState(
      serviceMode: serviceMode ?? this.serviceMode,
      portProxy: portProxy ?? this.portProxy,
      portLocalDns: portLocalDns ?? this.portLocalDns,
      portTransproxy: portTransproxy ?? this.portTransproxy,
      persistAcrossReboot: persistAcrossReboot ?? this.persistAcrossReboot,
      directBootAware: directBootAware ?? this.directBootAware,
    );
  }
}

final settingsProvider =
    AsyncNotifierProvider<SettingsNotifier, SettingsState>(
        SettingsNotifier.new);

class SettingsNotifier extends AsyncNotifier<SettingsState> {
  SettingsChannel get _channel => ref.read(settingsChannelProvider);

  @override
  Future<SettingsState> build() async {
    return SettingsState(
      serviceMode: await _channel.getServiceMode(),
      portProxy: await _channel.getPortProxy(),
      portLocalDns: await _channel.getPortLocalDns(),
      portTransproxy: await _channel.getPortTransproxy(),
      persistAcrossReboot: await _channel.getPersistAcrossReboot(),
      directBootAware: await _channel.getDirectBootAware(),
    );
  }

  Future<void> setServiceMode(String mode) async {
    await _channel.setServiceMode(mode);
    state = AsyncData(state.value!.copyWith(serviceMode: mode));
  }

  Future<void> setPortProxy(int port) async {
    await _channel.setPortProxy(port);
    state = AsyncData(state.value!.copyWith(portProxy: port));
  }

  Future<void> setPortLocalDns(int port) async {
    await _channel.setPortLocalDns(port);
    state = AsyncData(state.value!.copyWith(portLocalDns: port));
  }

  Future<void> setPortTransproxy(int port) async {
    await _channel.setPortTransproxy(port);
    state = AsyncData(state.value!.copyWith(portTransproxy: port));
  }

  Future<void> setPersistAcrossReboot(bool value) async {
    await _channel.setPersistAcrossReboot(value);
    state = AsyncData(state.value!.copyWith(persistAcrossReboot: value));
  }

  Future<void> setDirectBootAware(bool value) async {
    await _channel.setDirectBootAware(value);
    state = AsyncData(state.value!.copyWith(directBootAware: value));
  }
}

final appVersionProvider = FutureProvider<String>((ref) async {
  final channel = ref.read(settingsChannelProvider);
  return channel.getVersion();
});
