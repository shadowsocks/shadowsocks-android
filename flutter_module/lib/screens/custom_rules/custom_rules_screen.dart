import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final _aclChannelProvider = Provider(
  (ref) => const MethodChannel('com.github.shadowsocks/acl'),
);

class AclRules {
  final List<String> subnets;
  final List<String> hostnames;
  final List<String> urls;

  const AclRules({
    this.subnets = const [],
    this.hostnames = const [],
    this.urls = const [],
  });

  List<String> get all => [...subnets, ...hostnames, ...urls];
}

final aclRulesProvider =
    AsyncNotifierProvider<AclRulesNotifier, AclRules>(AclRulesNotifier.new);

class AclRulesNotifier extends AsyncNotifier<AclRules> {
  MethodChannel get _channel => ref.read(_aclChannelProvider);

  @override
  Future<AclRules> build() async {
    final result =
        await _channel.invokeMapMethod<String, dynamic>('getRules');
    if (result == null) return const AclRules();
    return AclRules(
      subnets:
          (result['subnets'] as List?)?.cast<String>() ?? [],
      hostnames:
          (result['hostnames'] as List?)?.cast<String>() ?? [],
      urls: (result['urls'] as List?)?.cast<String>() ?? [],
    );
  }

  Future<void> addRule(String rule, String type) async {
    await _channel.invokeMethod('addRule', {'rule': rule, 'type': type});
    state = await AsyncValue.guard(() => build());
  }

  Future<void> removeRule(String rule, String type) async {
    await _channel.invokeMethod('removeRule', {'rule': rule, 'type': type});
    state = await AsyncValue.guard(() => build());
  }
}

class CustomRulesScreen extends ConsumerWidget {
  const CustomRulesScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final rules = ref.watch(aclRulesProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('Custom Rules')),
      body: rules.when(
        data: (r) {
          final allRules = <_RuleEntry>[
            ...r.subnets.map((s) => _RuleEntry(s, 'subnet', Icons.lan_rounded)),
            ...r.hostnames
                .map((h) => _RuleEntry(h, 'hostname', Icons.language_rounded)),
            ...r.urls.map((u) => _RuleEntry(u, 'url', Icons.link_rounded)),
          ];
          if (allRules.isEmpty) {
            return Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.rule_rounded,
                      size: 64,
                      color: theme.colorScheme.onSurfaceVariant.withAlpha(77)),
                  const SizedBox(height: 16),
                  Text('No custom rules',
                      style: theme.textTheme.bodyLarge?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant)),
                ],
              ),
            );
          }
          return ListView.builder(
            padding: const EdgeInsets.symmetric(vertical: 8),
            itemCount: allRules.length,
            itemBuilder: (context, index) {
              final entry = allRules[index];
              return Dismissible(
                key: ValueKey('${entry.type}:${entry.rule}'),
                direction: DismissDirection.endToStart,
                background: Container(
                  alignment: Alignment.centerRight,
                  padding: const EdgeInsets.only(right: 24),
                  color: theme.colorScheme.error,
                  child: Icon(Icons.delete_outline_rounded,
                      color: theme.colorScheme.onError),
                ),
                onDismissed: (_) {
                  ref
                      .read(aclRulesProvider.notifier)
                      .removeRule(entry.rule, entry.type);
                },
                child: ListTile(
                  leading: CircleAvatar(
                    backgroundColor: theme.colorScheme.surfaceContainerHighest,
                    child: Icon(entry.icon,
                        size: 20,
                        color: theme.colorScheme.onSurfaceVariant),
                  ),
                  title: Text(entry.rule,
                      style: theme.textTheme.bodyMedium,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis),
                  subtitle: Text(entry.type,
                      style: theme.textTheme.labelSmall?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant)),
                ),
              );
            },
          );
        },
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (error, _) => Center(child: Text('Error: $error')),
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showAddDialog(context, ref),
        child: const Icon(Icons.add_rounded),
      ),
    );
  }

  void _showAddDialog(BuildContext context, WidgetRef ref) {
    final controller = TextEditingController();
    String type = 'hostname';

    showDialog(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, setState) => AlertDialog(
          title: const Text('Add Rule'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              SegmentedButton<String>(
                segments: const [
                  ButtonSegment(value: 'hostname', label: Text('Domain')),
                  ButtonSegment(value: 'subnet', label: Text('Subnet')),
                  ButtonSegment(value: 'url', label: Text('URL')),
                ],
                selected: {type},
                onSelectionChanged: (v) => setState(() => type = v.first),
              ),
              const SizedBox(height: 16),
              TextField(
                controller: controller,
                autofocus: true,
                decoration: InputDecoration(
                  labelText: type == 'hostname'
                      ? 'Domain'
                      : type == 'subnet'
                          ? 'CIDR Subnet'
                          : 'URL',
                  hintText: type == 'hostname'
                      ? 'example.com'
                      : type == 'subnet'
                          ? '192.168.0.0/16'
                          : 'https://...',
                ),
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () {
                final rule = controller.text.trim();
                if (rule.isNotEmpty) {
                  ref.read(aclRulesProvider.notifier).addRule(rule, type);
                  Navigator.of(context).pop();
                }
              },
              child: const Text('Add'),
            ),
          ],
        ),
      ),
    );
  }
}

class _RuleEntry {
  final String rule;
  final String type;
  final IconData icon;
  const _RuleEntry(this.rule, this.type, this.icon);
}
