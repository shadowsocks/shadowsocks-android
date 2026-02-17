import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../channels/traffic_channel.dart';
import '../models/traffic_stats.dart';

final trafficChannelProvider = Provider((ref) => TrafficChannel());

final trafficStatsProvider =
    StreamProvider.autoDispose<TrafficStats>((ref) {
  final channel = ref.watch(trafficChannelProvider);
  return channel.trafficStream;
});
