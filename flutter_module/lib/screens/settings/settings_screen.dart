import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../providers/settings_provider.dart';
import '../../providers/service_provider.dart';

class SettingsScreen extends ConsumerWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final settings = ref.watch(settingsProvider);
    final state = ref.watch(currentStateProvider);
    final isRunning = !state.canToggle || state.isStarted;
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: settings.when(
        data: (s) => ListView(
          padding: const EdgeInsets.symmetric(vertical: 8),
          children: [
            _sectionHeader(context, 'Service'),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
              child: DropdownButtonFormField<String>(
                initialValue: s.serviceMode,
                decoration: const InputDecoration(
                  labelText: 'Service Mode',
                  prefixIcon: Icon(Icons.miscellaneous_services_rounded),
                ),
                items: const [
                  DropdownMenuItem(value: 'vpn', child: Text('VPN')),
                  DropdownMenuItem(value: 'proxy', child: Text('Proxy Only')),
                  DropdownMenuItem(
                      value: 'transproxy',
                      child: Text('Transparent Proxy')),
                ],
                onChanged: isRunning
                    ? null
                    : (v) {
                        if (v != null) {
                          ref
                              .read(settingsProvider.notifier)
                              .setServiceMode(v);
                        }
                      },
              ),
            ),
            const SizedBox(height: 16),
            _sectionHeader(context, 'Ports'),
            _portTile(
              context,
              ref,
              label: 'SOCKS5 Proxy Port',
              value: s.portProxy,
              icon: Icons.lan_outlined,
              enabled: !isRunning,
              onChanged: (port) =>
                  ref.read(settingsProvider.notifier).setPortProxy(port),
            ),
            _portTile(
              context,
              ref,
              label: 'Local DNS Port',
              value: s.portLocalDns,
              icon: Icons.dns_outlined,
              enabled: !isRunning,
              onChanged: (port) =>
                  ref.read(settingsProvider.notifier).setPortLocalDns(port),
            ),
            if (s.serviceMode == 'transproxy')
              _portTile(
                context,
                ref,
                label: 'Transproxy Port',
                value: s.portTransproxy,
                icon: Icons.swap_horiz_rounded,
                enabled: !isRunning,
                onChanged: (port) => ref
                    .read(settingsProvider.notifier)
                    .setPortTransproxy(port),
              ),
            const SizedBox(height: 16),
            _sectionHeader(context, 'System'),
            SwitchListTile(
              title: const Text('Persist Across Reboot'),
              subtitle: const Text('Auto-connect on device startup'),
              secondary: const Icon(Icons.restart_alt_rounded),
              value: s.persistAcrossReboot,
              onChanged: isRunning
                  ? null
                  : (v) => ref
                      .read(settingsProvider.notifier)
                      .setPersistAcrossReboot(v),
            ),
            SwitchListTile(
              title: const Text('Direct Boot Aware'),
              subtitle: const Text('Connect before device unlock'),
              secondary: const Icon(Icons.lock_open_rounded),
              value: s.directBootAware,
              onChanged: isRunning
                  ? null
                  : (v) => ref
                      .read(settingsProvider.notifier)
                      .setDirectBootAware(v),
            ),
            if (isRunning) ...[
              const SizedBox(height: 24),
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: Card(
                  color: theme.colorScheme.tertiaryContainer,
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Row(
                      children: [
                        Icon(Icons.info_outline_rounded,
                            color: theme.colorScheme.onTertiaryContainer),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            'Stop the service to change settings.',
                            style: theme.textTheme.bodyMedium?.copyWith(
                              color: theme.colorScheme.onTertiaryContainer,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
          ],
        ),
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (error, _) => Center(child: Text('Error: $error')),
      ),
    );
  }

  Widget _sectionHeader(BuildContext context, String title) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 8, 16, 4),
      child: Text(
        title,
        style: theme.textTheme.titleSmall?.copyWith(
          color: theme.colorScheme.primary,
          fontWeight: FontWeight.w600,
          letterSpacing: 0.5,
        ),
      ),
    );
  }

  Widget _portTile(
    BuildContext context,
    WidgetRef ref, {
    required String label,
    required int value,
    required IconData icon,
    required bool enabled,
    required ValueChanged<int> onChanged,
  }) {
    return ListTile(
      leading: Icon(icon),
      title: Text(label),
      trailing: Text(
        value.toString(),
        style: Theme.of(context).textTheme.bodyLarge?.copyWith(
              color: Theme.of(context).colorScheme.primary,
              fontWeight: FontWeight.w600,
            ),
      ),
      enabled: enabled,
      onTap: enabled
          ? () => _showPortDialog(context, label, value, onChanged)
          : null,
    );
  }

  void _showPortDialog(BuildContext context, String label, int currentValue,
      ValueChanged<int> onChanged) {
    final controller = TextEditingController(text: currentValue.toString());
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(label),
        content: TextField(
          controller: controller,
          keyboardType: TextInputType.number,
          autofocus: true,
          decoration: const InputDecoration(
            labelText: 'Port',
            hintText: '1-65535',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () {
              final port = int.tryParse(controller.text);
              if (port != null && port >= 1 && port <= 65535) {
                onChanged(port);
                Navigator.of(context).pop();
              }
            },
            child: const Text('OK'),
          ),
        ],
      ),
    );
  }
}
