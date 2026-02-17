import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../channels/service_channel.dart';
import '../models/service_state.dart';

final serviceChannelProvider = Provider((ref) => ServiceChannel());

final serviceStateProvider =
    StreamProvider.autoDispose<ServiceStatus>((ref) async* {
  final channel = ref.watch(serviceChannelProvider);

  // Emit initial state
  final initialState = await channel.getState();
  yield ServiceStatus(state: initialState);

  // Then stream updates
  yield* channel.stateStream;
});

final currentStateProvider = Provider<ServiceState>((ref) {
  return ref.watch(serviceStateProvider).valueOrNull?.state ??
      ServiceState.idle;
});

final serviceToggleProvider = Provider<Future<bool> Function()>((ref) {
  final channel = ref.read(serviceChannelProvider);
  return () => channel.toggle();
});
