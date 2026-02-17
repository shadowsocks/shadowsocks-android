import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final _appListChannel = Provider(
  (ref) => const MethodChannel('com.github.shadowsocks/applist'),
);

class AppInfo {
  final String packageName;
  final String label;
  final int uid;
  bool isProxied;

  AppInfo({
    required this.packageName,
    required this.label,
    required this.uid,
    required this.isProxied,
  });

  factory AppInfo.fromMap(Map<dynamic, dynamic> map) {
    return AppInfo(
      packageName: map['package'] as String,
      label: (map['name'] as String?) ?? map['package'] as String,
      uid: (map['uid'] as int?) ?? 0,
      isProxied: (map['isProxied'] as bool?) ?? false,
    );
  }
}

class _AppListConfig {
  final bool enabled;
  final bool bypass;
  final List<AppInfo> apps;

  const _AppListConfig({
    this.enabled = false,
    this.bypass = false,
    this.apps = const [],
  });
}

final _appListProvider =
    AsyncNotifierProvider<_AppListNotifier, _AppListConfig>(
        _AppListNotifier.new);

class _AppListNotifier extends AsyncNotifier<_AppListConfig> {
  MethodChannel get _channel => ref.read(_appListChannel);

  @override
  Future<_AppListConfig> build() async {
    final config =
        await _channel.invokeMapMethod<String, dynamic>('getProxyAppsConfig');
    final appsRaw = await _channel.invokeListMethod<Map>('getApps');
    final apps = appsRaw?.map((m) => AppInfo.fromMap(m)).toList() ?? [];
    apps.sort((a, b) => a.label.toLowerCase().compareTo(b.label.toLowerCase()));
    return _AppListConfig(
      enabled: config?['enabled'] as bool? ?? false,
      bypass: config?['bypass'] as bool? ?? false,
      apps: apps,
    );
  }

  void toggleApp(String packageName) {
    final current = state.valueOrNull;
    if (current == null) return;
    final apps = current.apps;
    final idx = apps.indexWhere((a) => a.packageName == packageName);
    if (idx >= 0) {
      apps[idx].isProxied = !apps[idx].isProxied;
      state = AsyncData(_AppListConfig(
        enabled: current.enabled,
        bypass: current.bypass,
        apps: apps,
      ));
    }
  }

  Future<void> save() async {
    final current = state.valueOrNull;
    if (current == null) return;
    final packages = current.apps
        .where((a) => a.isProxied)
        .map((a) => a.packageName)
        .toList();
    await _channel.invokeMethod('setProxiedApps', {
      'packages': packages,
      'bypass': current.bypass,
    });
  }
}

class AppManagerScreen extends ConsumerStatefulWidget {
  const AppManagerScreen({super.key});

  @override
  ConsumerState<AppManagerScreen> createState() => _AppManagerScreenState();
}

class _AppManagerScreenState extends ConsumerState<AppManagerScreen> {
  String _search = '';
  final Map<String, Uint8List?> _iconCache = {};

  @override
  Widget build(BuildContext context) {
    final config = ref.watch(_appListProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Per-App Proxy'),
        actions: [
          IconButton(
            icon: const Icon(Icons.check_rounded),
            onPressed: () async {
              await ref.read(_appListProvider.notifier).save();
              if (mounted) Navigator.of(context).pop();
            },
          ),
        ],
      ),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: TextField(
              decoration: const InputDecoration(
                hintText: 'Search apps...',
                prefixIcon: Icon(Icons.search_rounded),
              ),
              onChanged: (v) => setState(() => _search = v),
            ),
          ),
          Expanded(
            child: config.when(
              data: (c) {
                final filtered = c.apps.where((a) {
                  if (_search.isEmpty) return true;
                  return a.label
                          .toLowerCase()
                          .contains(_search.toLowerCase()) ||
                      a.packageName
                          .toLowerCase()
                          .contains(_search.toLowerCase());
                }).toList();
                return ListView.builder(
                  itemCount: filtered.length,
                  itemBuilder: (context, index) {
                    final app = filtered[index];
                    return CheckboxListTile(
                      value: app.isProxied,
                      onChanged: (_) => ref
                          .read(_appListProvider.notifier)
                          .toggleApp(app.packageName),
                      title: Text(app.label, maxLines: 1, overflow: TextOverflow.ellipsis),
                      subtitle: Text(
                        app.packageName,
                        style: theme.textTheme.labelSmall?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant,
                        ),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                      secondary: _buildAppIcon(app.packageName),
                    );
                  },
                );
              },
              loading: () =>
                  const Center(child: CircularProgressIndicator()),
              error: (error, _) =>
                  Center(child: Text('Error: $error')),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildAppIcon(String packageName) {
    if (_iconCache.containsKey(packageName)) {
      final bytes = _iconCache[packageName];
      if (bytes != null) {
        return ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: Image.memory(bytes, width: 40, height: 40),
        );
      }
    } else {
      _loadIcon(packageName);
    }
    return const SizedBox(
      width: 40,
      height: 40,
      child: Icon(Icons.android_rounded),
    );
  }

  void _loadIcon(String packageName) async {
    _iconCache[packageName] = null; // mark loading
    final channel = ref.read(_appListChannel);
    try {
      final bytes = await channel
          .invokeMethod<Uint8List>('getAppIcon', {'package': packageName});
      if (mounted) {
        setState(() => _iconCache[packageName] = bytes);
      }
    } catch (_) {
      // icon loading failed, use placeholder
    }
  }
}
