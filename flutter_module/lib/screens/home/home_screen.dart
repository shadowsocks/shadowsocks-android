import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../models/service_state.dart';
import '../../providers/profiles_provider.dart';
import '../../providers/service_provider.dart';
import 'widgets/profile_card.dart';
import 'widgets/service_fab.dart';
import 'widgets/stats_bar.dart';

class HomeScreen extends ConsumerWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final profiles = ref.watch(profilesProvider);
    final selectedId = ref.watch(selectedProfileIdProvider);
    final state = ref.watch(currentStateProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Shadowsocks'),
        leading: Builder(
          builder: (context) => IconButton(
            icon: const Icon(Icons.menu_rounded),
            onPressed: () => Scaffold.of(context).openDrawer(),
          ),
        ),
        actions: [
          PopupMenuButton<String>(
            icon: const Icon(Icons.add_rounded),
            onSelected: (value) => _handleImport(context, ref, value),
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'scan',
                child: ListTile(
                  leading: Icon(Icons.qr_code_scanner_rounded),
                  title: Text('Scan QR Code'),
                  contentPadding: EdgeInsets.zero,
                  visualDensity: VisualDensity.compact,
                ),
              ),
              const PopupMenuItem(
                value: 'clipboard',
                child: ListTile(
                  leading: Icon(Icons.content_paste_rounded),
                  title: Text('Import from Clipboard'),
                  contentPadding: EdgeInsets.zero,
                  visualDensity: VisualDensity.compact,
                ),
              ),
              const PopupMenuItem(
                value: 'manual',
                child: ListTile(
                  leading: Icon(Icons.edit_note_rounded),
                  title: Text('Manual Settings'),
                  contentPadding: EdgeInsets.zero,
                  visualDensity: VisualDensity.compact,
                ),
              ),
            ],
          ),
          PopupMenuButton<String>(
            onSelected: (value) => _handleMenu(context, ref, value),
            itemBuilder: (context) => [
              const PopupMenuItem(
                value: 'export',
                child: Text('Export to Clipboard'),
              ),
              const PopupMenuItem(
                value: 'sort_name',
                child: Text('Sort by Name'),
              ),
            ],
          ),
        ],
      ),
      drawer: _buildDrawer(context),
      body: Column(
        children: [
          Expanded(
            child: profiles.when(
              data: (list) {
                if (list.isEmpty) {
                  return _buildEmptyState(context);
                }
                return ListView.builder(
                  padding: const EdgeInsets.only(top: 8, bottom: 100),
                  itemCount: list.length,
                  itemBuilder: (context, index) {
                    final profile = list[index];
                    final isSelected =
                        selectedId.valueOrNull == profile.id;
                    return Dismissible(
                      key: ValueKey(profile.id),
                      direction: DismissDirection.endToStart,
                      background: Container(
                        alignment: Alignment.centerRight,
                        padding: const EdgeInsets.only(right: 24),
                        color: theme.colorScheme.error,
                        child: Icon(Icons.delete_outline_rounded,
                            color: theme.colorScheme.onError),
                      ),
                      confirmDismiss: (direction) async {
                        return await _confirmDelete(context, profile.displayName);
                      },
                      onDismissed: (_) {
                        ref
                            .read(profilesProvider.notifier)
                            .deleteProfile(profile.id);
                        ScaffoldMessenger.of(context).showSnackBar(
                          SnackBar(
                            content:
                                Text('${profile.displayName} deleted'),
                            action: SnackBarAction(
                              label: 'Undo',
                              onPressed: () {
                                ref
                                    .read(profilesProvider.notifier)
                                    .createProfile(profile.toMap());
                              },
                            ),
                          ),
                        );
                      },
                      child: ProfileCard(
                        profile: profile,
                        isSelected: isSelected,
                        isConnected:
                            isSelected && state == ServiceState.connected,
                        onTap: () => ref
                            .read(profilesProvider.notifier)
                            .selectProfile(profile.id),
                        onEdit: () => context
                            .push('/profile/${profile.id}'),
                        onShare: () => _shareProfile(context, ref, profile.id),
                      ),
                    );
                  },
                );
              },
              loading: () => const Center(
                child: CircularProgressIndicator(),
              ),
              error: (error, _) => Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.error_outline_rounded,
                        size: 48,
                        color: theme.colorScheme.error),
                    const SizedBox(height: 16),
                    Text('Failed to load profiles',
                        style: theme.textTheme.bodyLarge),
                    const SizedBox(height: 8),
                    FilledButton.tonal(
                      onPressed: () =>
                          ref.read(profilesProvider.notifier).refresh(),
                      child: const Text('Retry'),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const StatsBar(),
        ],
      ),
      floatingActionButton: const ServiceFab(),
      floatingActionButtonLocation: FloatingActionButtonLocation.centerFloat,
    );
  }

  Widget _buildEmptyState(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(48),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.vpn_key_off_rounded,
              size: 80,
              color: theme.colorScheme.onSurfaceVariant.withAlpha(77),
            ),
            const SizedBox(height: 24),
            Text(
              'No profiles yet',
              style: theme.textTheme.headlineSmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'Add a server profile to get started.\nTap + to scan a QR code or enter manually.',
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurfaceVariant.withAlpha(153),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDrawer(BuildContext context) {
    final theme = Theme.of(context);
    return NavigationDrawer(
      selectedIndex: 0,
      onDestinationSelected: (index) {
        Navigator.of(context).pop();
        switch (index) {
          case 0:
            break; // already on profiles
          case 1:
            context.push('/subscriptions');
          case 2:
            context.push('/custom-rules');
          case 3:
            context.push('/settings');
          case 4:
            context.push('/about');
        }
      },
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(28, 24, 16, 16),
          child: Text(
            'Shadowsocks',
            style: theme.textTheme.titleLarge?.copyWith(
              fontWeight: FontWeight.w700,
              color: theme.colorScheme.primary,
            ),
          ),
        ),
        const Divider(indent: 28, endIndent: 28),
        const SizedBox(height: 8),
        const NavigationDrawerDestination(
          icon: Icon(Icons.vpn_key_outlined),
          selectedIcon: Icon(Icons.vpn_key),
          label: Text('Profiles'),
        ),
        const NavigationDrawerDestination(
          icon: Icon(Icons.sync_outlined),
          selectedIcon: Icon(Icons.sync),
          label: Text('Subscriptions'),
        ),
        const NavigationDrawerDestination(
          icon: Icon(Icons.rule_outlined),
          selectedIcon: Icon(Icons.rule),
          label: Text('Custom Rules'),
        ),
        const NavigationDrawerDestination(
          icon: Icon(Icons.settings_outlined),
          selectedIcon: Icon(Icons.settings),
          label: Text('Settings'),
        ),
        const NavigationDrawerDestination(
          icon: Icon(Icons.info_outline_rounded),
          selectedIcon: Icon(Icons.info_rounded),
          label: Text('About'),
        ),
      ],
    );
  }

  void _handleImport(BuildContext context, WidgetRef ref, String value) async {
    switch (value) {
      case 'scan':
        context.push('/scanner');
      case 'clipboard':
        final data = await Clipboard.getData(Clipboard.kTextPlain);
        if (data?.text != null && data!.text!.isNotEmpty) {
          final count = await ref
              .read(profilesProvider.notifier)
              .importFromText(data.text!);
          if (context.mounted) {
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(count > 0
                    ? 'Imported $count profile(s)'
                    : 'No valid profiles found'),
              ),
            );
          }
        }
      case 'manual':
        context.push('/profile/new');
    }
  }

  void _handleMenu(BuildContext context, WidgetRef ref, String value) async {
    if (value == 'export') {
      final json = await ref.read(profilesProvider.notifier).exportToJson();
      await Clipboard.setData(ClipboardData(text: json));
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Profiles exported to clipboard')),
        );
      }
    }
  }

  Future<bool?> _confirmDelete(BuildContext context, String name) {
    return showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Profile'),
        content: Text('Delete "$name"?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
  }

  void _shareProfile(BuildContext context, WidgetRef ref, int id) async {
    final channel = ref.read(profileChannelProvider);
    final uri = await channel.getProfileUri(id);
    if (uri != null && context.mounted) {
      await Clipboard.setData(ClipboardData(text: uri));
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Profile URI copied to clipboard')),
      );
    }
  }
}
