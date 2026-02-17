import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../providers/settings_provider.dart';

class AboutScreen extends ConsumerWidget {
  const AboutScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final version = ref.watch(appVersionProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('About')),
      body: ListView(
        padding: const EdgeInsets.all(24),
        children: [
          const SizedBox(height: 32),
          // App icon and name
          Center(
            child: Container(
              width: 96,
              height: 96,
              decoration: BoxDecoration(
                color: theme.colorScheme.primaryContainer,
                borderRadius: BorderRadius.circular(24),
              ),
              child: Icon(
                Icons.vpn_key_rounded,
                size: 48,
                color: theme.colorScheme.onPrimaryContainer,
              ),
            ),
          ),
          const SizedBox(height: 24),
          Text(
            'Shadowsocks',
            textAlign: TextAlign.center,
            style: theme.textTheme.headlineMedium?.copyWith(
              fontWeight: FontWeight.w700,
              color: theme.colorScheme.onSurface,
            ),
          ),
          const SizedBox(height: 4),
          version.when(
            data: (v) => Text(
              'v$v',
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyLarge?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
            loading: () => const SizedBox.shrink(),
            error: (_, __) => const SizedBox.shrink(),
          ),
          const SizedBox(height: 8),
          Text(
            'A secure SOCKS5 proxy for Android',
            textAlign: TextAlign.center,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant.withAlpha(179),
            ),
          ),
          const SizedBox(height: 48),
          // Info cards
          Card(
            child: Column(
              children: [
                ListTile(
                  leading: Icon(Icons.code_rounded,
                      color: theme.colorScheme.primary),
                  title: const Text('Source Code'),
                  subtitle: const Text('github.com/shadowsocks/shadowsocks-android'),
                  trailing: const Icon(Icons.open_in_new_rounded, size: 18),
                  onTap: () {
                    // TODO: open URL via platform channel
                  },
                ),
                const Divider(height: 1, indent: 56),
                ListTile(
                  leading: Icon(Icons.description_rounded,
                      color: theme.colorScheme.primary),
                  title: const Text('Open Source Licenses'),
                  trailing: const Icon(Icons.chevron_right_rounded),
                  onTap: () => showLicensePage(
                    context: context,
                    applicationName: 'Shadowsocks',
                  ),
                ),
                const Divider(height: 1, indent: 56),
                ListTile(
                  leading: Icon(Icons.help_outline_rounded,
                      color: theme.colorScheme.primary),
                  title: const Text('FAQ'),
                  trailing: const Icon(Icons.open_in_new_rounded, size: 18),
                  onTap: () {
                    // TODO: open FAQ URL
                  },
                ),
              ],
            ),
          ),
          const SizedBox(height: 24),
          Text(
            'Copyright (C) 2017 by Max Lv & Mygod Studio\n'
            'Licensed under GPLv3',
            textAlign: TextAlign.center,
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant.withAlpha(128),
            ),
          ),
        ],
      ),
    );
  }
}
