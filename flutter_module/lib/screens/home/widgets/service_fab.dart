import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../models/service_state.dart';
import '../../../providers/service_provider.dart';

class ServiceFab extends ConsumerWidget {
  const ServiceFab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final state = ref.watch(currentStateProvider);
    final theme = Theme.of(context);

    return SizedBox(
      width: 72,
      height: 72,
      child: Stack(
        alignment: Alignment.center,
        children: [
          // Progress ring for busy states
          if (state.isBusy)
            SizedBox(
              width: 72,
              height: 72,
              child: CircularProgressIndicator(
                strokeWidth: 3,
                valueColor: AlwaysStoppedAnimation(
                  theme.colorScheme.secondary.withAlpha(179),
                ),
              ),
            ),
          // Main FAB
          FloatingActionButton.large(
            onPressed: state.canToggle
                ? () => ref.read(serviceToggleProvider)()
                : null,
            backgroundColor: _fabColor(state, theme),
            elevation: state.isBusy ? 0 : 4,
            child: AnimatedSwitcher(
              duration: const Duration(milliseconds: 200),
              child: Icon(
                _fabIcon(state),
                key: ValueKey(state),
                size: 32,
                color: Colors.white,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Color _fabColor(ServiceState state, ThemeData theme) {
    switch (state) {
      case ServiceState.connected:
        return theme.colorScheme.secondary;
      case ServiceState.connecting:
      case ServiceState.stopping:
        return theme.colorScheme.secondary.withAlpha(153);
      default:
        return theme.colorScheme.surfaceContainerHighest;
    }
  }

  IconData _fabIcon(ServiceState state) {
    switch (state) {
      case ServiceState.connected:
        return Icons.flash_on_rounded;
      case ServiceState.connecting:
        return Icons.hourglass_top_rounded;
      case ServiceState.stopping:
        return Icons.hourglass_bottom_rounded;
      default:
        return Icons.flash_off_rounded;
    }
  }
}
