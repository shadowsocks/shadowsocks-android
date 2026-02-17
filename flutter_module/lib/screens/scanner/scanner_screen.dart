import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../providers/profiles_provider.dart';

/// QR scanner that delegates to the native Android scanner via platform channel.
/// This avoids the need for camera-related Flutter plugins.
class ScannerScreen extends ConsumerStatefulWidget {
  const ScannerScreen({super.key});

  @override
  ConsumerState<ScannerScreen> createState() => _ScannerScreenState();
}

class _ScannerScreenState extends ConsumerState<ScannerScreen> {
  static const _channel = MethodChannel('com.github.shadowsocks/scanner');

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  Future<void> _startScan() async {
    try {
      final result = await _channel.invokeMethod<String>('scan');
      if (result != null && result.isNotEmpty && mounted) {
        final count =
            await ref.read(profilesProvider.notifier).importFromText(result);
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(count > 0
                  ? 'Imported $count profile(s)'
                  : 'No valid profiles found'),
            ),
          );
          context.pop();
        }
      } else if (mounted) {
        context.pop();
      }
    } on PlatformException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Scanner error: ${e.message}')),
        );
        context.pop();
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Scaffold(
      appBar: AppBar(title: const Text('Scan QR Code')),
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(color: theme.colorScheme.secondary),
            const SizedBox(height: 24),
            Text(
              'Opening camera...',
              style: theme.textTheme.bodyLarge?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
