import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../models/profile.dart';
import '../../providers/profiles_provider.dart';

class ProfileConfigScreen extends ConsumerStatefulWidget {
  final String? profileId;

  const ProfileConfigScreen({super.key, this.profileId});

  bool get isNew => profileId == null || profileId == 'new';

  @override
  ConsumerState<ProfileConfigScreen> createState() =>
      _ProfileConfigScreenState();
}

class _ProfileConfigScreenState extends ConsumerState<ProfileConfigScreen> {
  final _formKey = GlobalKey<FormState>();
  late TextEditingController _nameCtrl;
  late TextEditingController _hostCtrl;
  late TextEditingController _portCtrl;
  late TextEditingController _passwordCtrl;
  late TextEditingController _remoteDnsCtrl;
  late TextEditingController _pluginCtrl;
  late TextEditingController _pluginOptsCtrl;
  String _method = 'chacha20-ietf-poly1305';
  String _route = 'all';
  bool _ipv6 = false;
  bool _metered = false;
  bool _udpdns = false;
  bool _isDirty = false;
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _nameCtrl = TextEditingController();
    _hostCtrl = TextEditingController();
    _portCtrl = TextEditingController(text: '8388');
    _passwordCtrl = TextEditingController();
    _remoteDnsCtrl = TextEditingController(text: 'dns.google');
    _pluginCtrl = TextEditingController();
    _pluginOptsCtrl = TextEditingController();
    _loadProfile();
  }

  Future<void> _loadProfile() async {
    if (!widget.isNew) {
      final channel = ref.read(profileChannelProvider);
      final profile = await channel.getProfile(int.parse(widget.profileId!));
      if (profile != null && mounted) {
        setState(() {
          _nameCtrl.text = profile.name;
          _hostCtrl.text = profile.host;
          _portCtrl.text = profile.remotePort.toString();
          _passwordCtrl.text = profile.password;
          _remoteDnsCtrl.text = profile.remoteDns;
          _method = profile.method;
          _route = profile.route;
          _ipv6 = profile.ipv6;
          _metered = profile.metered;
          _udpdns = profile.udpdns;
          _parsePlugin(profile.plugin);
        });
      }
    }
    setState(() => _loading = false);
  }

  @override
  void dispose() {
    _nameCtrl.dispose();
    _hostCtrl.dispose();
    _portCtrl.dispose();
    _passwordCtrl.dispose();
    _remoteDnsCtrl.dispose();
    _pluginCtrl.dispose();
    _pluginOptsCtrl.dispose();
    super.dispose();
  }

  void _markDirty() {
    if (!_isDirty) setState(() => _isDirty = true);
  }

  Future<bool> _onWillPop() async {
    if (!_isDirty) return true;
    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Discard changes?'),
        content: const Text('You have unsaved changes.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Keep Editing'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Discard'),
          ),
        ],
      ),
    );
    return result ?? false;
  }

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;

    final data = {
      'name': _nameCtrl.text,
      'host': _hostCtrl.text,
      'remotePort': int.tryParse(_portCtrl.text) ?? 8388,
      'password': _passwordCtrl.text,
      'method': _method,
      'route': _route,
      'remoteDns': _remoteDnsCtrl.text,
      'ipv6': _ipv6,
      'metered': _metered,
      'udpdns': _udpdns,
      'plugin': _buildPluginString(),
    };

    if (widget.isNew) {
      await ref.read(profilesProvider.notifier).createProfile(data);
    } else {
      data['id'] = int.parse(widget.profileId!);
      await ref.read(profilesProvider.notifier).updateProfile(data);
    }

    if (mounted) {
      _isDirty = false;
      context.pop();
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return PopScope(
      canPop: !_isDirty,
      onPopInvokedWithResult: (didPop, _) async {
        if (didPop) return;
        final shouldPop = await _onWillPop();
        if (shouldPop && mounted) context.pop();
      },
      child: Scaffold(
        appBar: AppBar(
          title: Text(widget.isNew ? 'New Profile' : 'Edit Profile'),
          actions: [
            FilledButton.icon(
              onPressed: _save,
              icon: const Icon(Icons.check_rounded, size: 18),
              label: const Text('Save'),
              style: FilledButton.styleFrom(
                visualDensity: VisualDensity.compact,
              ),
            ),
            const SizedBox(width: 12),
          ],
        ),
        body: _loading
            ? const Center(child: CircularProgressIndicator())
            : Form(
                key: _formKey,
                onChanged: _markDirty,
                child: ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    _sectionHeader('Server'),
                    const SizedBox(height: 8),
                    TextFormField(
                      controller: _nameCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Profile Name',
                        hintText: 'Optional',
                        prefixIcon: Icon(Icons.label_outline_rounded),
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextFormField(
                      controller: _hostCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Server Address',
                        hintText: 'hostname or IP',
                        prefixIcon: Icon(Icons.dns_outlined),
                      ),
                      validator: (v) =>
                          v == null || v.isEmpty ? 'Required' : null,
                    ),
                    const SizedBox(height: 12),
                    TextFormField(
                      controller: _portCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Server Port',
                        prefixIcon: Icon(Icons.numbers_rounded),
                      ),
                      keyboardType: TextInputType.number,
                      validator: (v) {
                        final port = int.tryParse(v ?? '');
                        if (port == null || port < 1 || port > 65535) {
                          return 'Enter a valid port (1-65535)';
                        }
                        return null;
                      },
                    ),
                    const SizedBox(height: 12),
                    TextFormField(
                      controller: _passwordCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Password',
                        prefixIcon: Icon(Icons.lock_outline_rounded),
                      ),
                      obscureText: true,
                      validator: (v) =>
                          v == null || v.isEmpty ? 'Required' : null,
                    ),
                    const SizedBox(height: 24),
                    _sectionHeader('Encryption'),
                    const SizedBox(height: 8),
                    DropdownButtonFormField<String>(
                      initialValue: _method,
                      decoration: const InputDecoration(
                        labelText: 'Encrypt Method',
                        prefixIcon: Icon(Icons.security_rounded),
                      ),
                      items: Profile.encryptionMethods
                          .map((m) => DropdownMenuItem(
                                value: m,
                                child: Text(m,
                                    style:
                                        theme.textTheme.bodyMedium),
                              ))
                          .toList(),
                      onChanged: (v) {
                        if (v != null) {
                          setState(() => _method = v);
                          _markDirty();
                        }
                      },
                    ),
                    const SizedBox(height: 24),
                    _sectionHeader('Routing'),
                    const SizedBox(height: 8),
                    DropdownButtonFormField<String>(
                      initialValue: _route,
                      decoration: const InputDecoration(
                        labelText: 'Route',
                        prefixIcon: Icon(Icons.alt_route_rounded),
                      ),
                      items: Profile.routeOptions.entries
                          .map((e) => DropdownMenuItem(
                                value: e.key,
                                child: Text(e.value),
                              ))
                          .toList(),
                      onChanged: (v) {
                        if (v != null) {
                          setState(() => _route = v);
                          _markDirty();
                        }
                      },
                    ),
                    const SizedBox(height: 12),
                    TextFormField(
                      controller: _remoteDnsCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Remote DNS',
                        prefixIcon: Icon(Icons.dns_rounded),
                      ),
                    ),
                    const SizedBox(height: 24),
                    _sectionHeader('Plugin'),
                    const SizedBox(height: 8),
                    TextFormField(
                      controller: _pluginCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Plugin',
                        hintText: 'e.g. v2ray-plugin, obfs-local',
                        prefixIcon: Icon(Icons.extension_rounded),
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextFormField(
                      controller: _pluginOptsCtrl,
                      decoration: const InputDecoration(
                        labelText: 'Plugin Options',
                        hintText: 'key=value;key=value',
                        prefixIcon: Icon(Icons.tune_rounded),
                      ),
                      maxLines: null,
                    ),
                    const SizedBox(height: 24),
                    _sectionHeader('Options'),
                    const SizedBox(height: 8),
                    _switchTile(
                      'IPv6 Route',
                      'Route IPv6 traffic through the proxy',
                      _ipv6,
                      (v) => setState(() {
                        _ipv6 = v;
                        _markDirty();
                      }),
                    ),
                    _switchTile(
                      'Metered Network',
                      'Treat VPN as metered connection',
                      _metered,
                      (v) => setState(() {
                        _metered = v;
                        _markDirty();
                      }),
                    ),
                    _switchTile(
                      'UDP over TCP',
                      'Send UDP DNS queries via TCP',
                      _udpdns,
                      (v) => setState(() {
                        _udpdns = v;
                        _markDirty();
                      }),
                    ),
                    const SizedBox(height: 12),
                    ListTile(
                      leading: const Icon(Icons.apps_rounded),
                      title: const Text('Per-App Proxy'),
                      subtitle: const Text('Select apps to proxy'),
                      trailing: const Icon(Icons.chevron_right_rounded),
                      onTap: () => context.push('/app-manager'),
                    ),
                    const SizedBox(height: 80),
                  ],
                ),
              ),
      ),
    );
  }

  Widget _sectionHeader(String title) {
    final theme = Theme.of(context);
    return Text(
      title,
      style: theme.textTheme.titleSmall?.copyWith(
        color: theme.colorScheme.primary,
        fontWeight: FontWeight.w600,
        letterSpacing: 0.5,
      ),
    );
  }

  void _parsePlugin(String? plugin) {
    if (plugin == null || plugin.isEmpty) return;
    final semicolon = plugin.indexOf(';');
    if (semicolon < 0) {
      _pluginCtrl.text = plugin;
    } else {
      _pluginCtrl.text = plugin.substring(0, semicolon);
      _pluginOptsCtrl.text = plugin.substring(semicolon + 1);
    }
  }

  String _buildPluginString() {
    final id = _pluginCtrl.text.trim();
    if (id.isEmpty) return '';
    final opts = _pluginOptsCtrl.text.trim();
    return opts.isEmpty ? id : '$id;$opts';
  }

  Widget _switchTile(
      String title, String subtitle, bool value, ValueChanged<bool> onChanged) {
    return SwitchListTile(
      title: Text(title),
      subtitle: Text(subtitle),
      value: value,
      onChanged: onChanged,
      contentPadding: const EdgeInsets.symmetric(horizontal: 4),
    );
  }
}
