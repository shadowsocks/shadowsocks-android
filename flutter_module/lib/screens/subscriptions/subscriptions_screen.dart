import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

final _subscriptionChannelProvider = Provider(
  (ref) => const MethodChannel('com.github.shadowsocks/subscriptions'),
);

final subscriptionsProvider =
    AsyncNotifierProvider<SubscriptionsNotifier, List<String>>(
        SubscriptionsNotifier.new);

class SubscriptionsNotifier extends AsyncNotifier<List<String>> {
  MethodChannel get _channel => ref.read(_subscriptionChannelProvider);

  @override
  Future<List<String>> build() async {
    final result = await _channel.invokeListMethod<String>('getSubscriptions');
    return result ?? [];
  }

  Future<void> add(String url) async {
    await _channel.invokeMethod('addSubscription', {'url': url});
    state = await AsyncValue.guard(() => build());
  }

  Future<void> remove(String url) async {
    await _channel.invokeMethod('removeSubscription', {'url': url});
    state = await AsyncValue.guard(() => build());
  }

  Future<void> updateAll() async {
    await _channel.invokeMethod('updateSubscriptions');
  }
}

class SubscriptionsScreen extends ConsumerWidget {
  const SubscriptionsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final subs = ref.watch(subscriptionsProvider);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Subscriptions'),
        actions: [
          IconButton(
            icon: const Icon(Icons.sync_rounded),
            tooltip: 'Update All',
            onPressed: () {
              ref.read(subscriptionsProvider.notifier).updateAll();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Updating subscriptions...')),
              );
            },
          ),
        ],
      ),
      body: subs.when(
        data: (list) {
          if (list.isEmpty) {
            return Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.sync_disabled_rounded,
                      size: 64,
                      color: theme.colorScheme.onSurfaceVariant.withAlpha(77)),
                  const SizedBox(height: 16),
                  Text('No subscriptions',
                      style: theme.textTheme.bodyLarge?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant)),
                  const SizedBox(height: 8),
                  Text('Add a subscription URL to auto-update profiles.',
                      style: theme.textTheme.bodyMedium?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant
                              .withAlpha(153))),
                ],
              ),
            );
          }
          return ListView.builder(
            padding: const EdgeInsets.symmetric(vertical: 8),
            itemCount: list.length,
            itemBuilder: (context, index) {
              final url = list[index];
              return Dismissible(
                key: ValueKey(url),
                direction: DismissDirection.endToStart,
                background: Container(
                  alignment: Alignment.centerRight,
                  padding: const EdgeInsets.only(right: 24),
                  color: theme.colorScheme.error,
                  child: Icon(Icons.delete_outline_rounded,
                      color: theme.colorScheme.onError),
                ),
                onDismissed: (_) {
                  ref.read(subscriptionsProvider.notifier).remove(url);
                },
                child: ListTile(
                  leading: CircleAvatar(
                    backgroundColor: theme.colorScheme.primaryContainer,
                    child: Icon(Icons.link_rounded,
                        color: theme.colorScheme.onPrimaryContainer),
                  ),
                  title: Text(url, maxLines: 2, overflow: TextOverflow.ellipsis),
                  contentPadding:
                      const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
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
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Add Subscription'),
        content: TextField(
          controller: controller,
          autofocus: true,
          decoration: const InputDecoration(
            labelText: 'Subscription URL',
            hintText: 'https://...',
            prefixIcon: Icon(Icons.link_rounded),
          ),
          keyboardType: TextInputType.url,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () {
              final url = controller.text.trim();
              if (url.isNotEmpty) {
                ref.read(subscriptionsProvider.notifier).add(url);
                Navigator.of(context).pop();
              }
            },
            child: const Text('Add'),
          ),
        ],
      ),
    );
  }
}
