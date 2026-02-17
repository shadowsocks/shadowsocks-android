import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../models/traffic_stats.dart';
import '../../../providers/service_provider.dart';
import '../../../providers/traffic_provider.dart';

class StatsBar extends ConsumerWidget {
  const StatsBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final state = ref.watch(currentStateProvider);
    final traffic = ref.watch(trafficStatsProvider);
    final theme = Theme.of(context);

    return AnimatedContainer(
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeInOut,
      height: state.isStarted ? 56 : 0,
      clipBehavior: Clip.antiAlias,
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest.withAlpha(128),
        border: Border(
          top: BorderSide(
            color: theme.colorScheme.outlineVariant.withAlpha(77),
          ),
        ),
      ),
      child: traffic.when(
        data: (stats) => _StatsContent(stats: stats),
        loading: () => const _StatsContent(stats: TrafficStats()),
        error: (_, __) => const _StatsContent(stats: TrafficStats()),
      ),
    );
  }
}

class _StatsContent extends StatelessWidget {
  final TrafficStats stats;

  const _StatsContent({required this.stats});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 24),
      child: Row(
        children: [
          // Upload
          Icon(
            Icons.arrow_upward_rounded,
            size: 16,
            color: theme.colorScheme.primary,
          ),
          const SizedBox(width: 4),
          Text(
            TrafficStats.formatSpeed(stats.txRate),
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
          const SizedBox(width: 8),
          Text(
            TrafficStats.formatBytes(stats.txTotal),
            style: theme.textTheme.labelSmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant.withAlpha(153),
            ),
          ),
          const Spacer(),
          // Download
          Text(
            TrafficStats.formatBytes(stats.rxTotal),
            style: theme.textTheme.labelSmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant.withAlpha(153),
            ),
          ),
          const SizedBox(width: 8),
          Text(
            TrafficStats.formatSpeed(stats.rxRate),
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
              fontFeatures: const [FontFeature.tabularFigures()],
            ),
          ),
          const SizedBox(width: 4),
          Icon(
            Icons.arrow_downward_rounded,
            size: 16,
            color: theme.colorScheme.secondary,
          ),
        ],
      ),
    );
  }
}
