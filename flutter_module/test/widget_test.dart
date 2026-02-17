import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_module/models/profile.dart';
import 'package:flutter_module/models/service_state.dart';
import 'package:flutter_module/models/traffic_stats.dart';

void main() {
  group('Profile', () {
    test('fromMap/toMap round-trip preserves all fields', () {
      final map = {
        'id': 42,
        'name': 'My Server',
        'host': '192.168.1.1',
        'remotePort': 9999,
        'password': 'secret',
        'method': 'aes-256-gcm',
        'route': 'bypass-lan',
        'remoteDns': '8.8.8.8',
        'proxyApps': true,
        'bypass': true,
        'udpdns': true,
        'ipv6': true,
        'metered': true,
        'individual': 'com.example.app',
        'plugin': 'v2ray-plugin',
        'udpFallback': 3,
        'subscription': 1,
        'tx': 1000,
        'rx': 2000,
        'userOrder': 5,
      };
      final profile = Profile.fromMap(map);
      final result = profile.toMap();

      expect(result['id'], 42);
      expect(result['name'], 'My Server');
      expect(result['host'], '192.168.1.1');
      expect(result['remotePort'], 9999);
      expect(result['password'], 'secret');
      expect(result['method'], 'aes-256-gcm');
      expect(result['route'], 'bypass-lan');
      expect(result['remoteDns'], '8.8.8.8');
      expect(result['proxyApps'], true);
      expect(result['bypass'], true);
      expect(result['udpdns'], true);
      expect(result['ipv6'], true);
      expect(result['metered'], true);
      expect(result['individual'], 'com.example.app');
      expect(result['plugin'], 'v2ray-plugin');
      expect(result['udpFallback'], 3);
      expect(result['subscription'], 1);
      expect(result['tx'], 1000);
      expect(result['rx'], 2000);
      expect(result['userOrder'], 5);
    });

    test('fromMap applies defaults for missing fields', () {
      final profile = Profile.fromMap({'id': 1});
      expect(profile.name, '');
      expect(profile.host, '');
      expect(profile.remotePort, 8388);
      expect(profile.password, '');
      expect(profile.method, 'chacha20-ietf-poly1305');
      expect(profile.route, 'all');
      expect(profile.remoteDns, 'dns.google');
      expect(profile.proxyApps, false);
      expect(profile.bypass, false);
      expect(profile.plugin, isNull);
      expect(profile.udpFallback, isNull);
    });

    test('displayName returns name when set', () {
      final profile = Profile(
        id: 1,
        host: '1.2.3.4',
        password: 'pw',
        name: 'Tokyo',
      );
      expect(profile.displayName, 'Tokyo');
    });

    test('displayName falls back to host:port when name is empty', () {
      final profile = Profile(
        id: 1,
        host: '1.2.3.4',
        remotePort: 443,
        password: 'pw',
      );
      expect(profile.displayName, '1.2.3.4:443');
    });

    test('encryptionMethods is non-empty and contains common ciphers', () {
      expect(Profile.encryptionMethods, isNotEmpty);
      expect(Profile.encryptionMethods, contains('aes-256-gcm'));
      expect(Profile.encryptionMethods, contains('chacha20-ietf-poly1305'));
    });

    test('routeOptions contains expected keys', () {
      expect(Profile.routeOptions, containsPair('all', 'All'));
      expect(Profile.routeOptions, containsPair('bypass-lan', 'Bypass LAN'));
      expect(Profile.routeOptions.length, 7);
    });
  });

  group('ServiceState', () {
    test('fromValue returns correct enum for each value', () {
      expect(ServiceState.fromValue(0), ServiceState.idle);
      expect(ServiceState.fromValue(1), ServiceState.connecting);
      expect(ServiceState.fromValue(2), ServiceState.connected);
      expect(ServiceState.fromValue(3), ServiceState.stopping);
      expect(ServiceState.fromValue(4), ServiceState.stopped);
    });

    test('fromValue defaults to idle for unknown values', () {
      expect(ServiceState.fromValue(99), ServiceState.idle);
      expect(ServiceState.fromValue(-1), ServiceState.idle);
    });

    test('isStarted is true only for connected', () {
      expect(ServiceState.connected.isStarted, true);
      expect(ServiceState.idle.isStarted, false);
      expect(ServiceState.connecting.isStarted, false);
      expect(ServiceState.stopping.isStarted, false);
      expect(ServiceState.stopped.isStarted, false);
    });

    test('isBusy is true for connecting and stopping', () {
      expect(ServiceState.connecting.isBusy, true);
      expect(ServiceState.stopping.isBusy, true);
      expect(ServiceState.idle.isBusy, false);
      expect(ServiceState.connected.isBusy, false);
      expect(ServiceState.stopped.isBusy, false);
    });

    test('canToggle is inverse of isBusy', () {
      for (final state in ServiceState.values) {
        expect(state.canToggle, !state.isBusy);
      }
    });
  });

  group('ServiceStatus', () {
    test('fromMap deserializes all fields', () {
      final status = ServiceStatus.fromMap({
        'state': 2,
        'profileName': 'Tokyo',
        'message': 'Connected',
      });
      expect(status.state, ServiceState.connected);
      expect(status.profileName, 'Tokyo');
      expect(status.message, 'Connected');
    });

    test('fromMap handles missing optional fields', () {
      final status = ServiceStatus.fromMap({'state': 0});
      expect(status.state, ServiceState.idle);
      expect(status.profileName, isNull);
      expect(status.message, isNull);
    });

    test('fromMap defaults state to idle when missing', () {
      final status = ServiceStatus.fromMap({});
      expect(status.state, ServiceState.idle);
    });
  });

  group('TrafficStats', () {
    test('fromMap deserializes all fields', () {
      final stats = TrafficStats.fromMap({
        'profileId': 1,
        'txRate': 100,
        'rxRate': 200,
        'txTotal': 1000,
        'rxTotal': 2000,
      });
      expect(stats.profileId, 1);
      expect(stats.txRate, 100);
      expect(stats.rxRate, 200);
      expect(stats.txTotal, 1000);
      expect(stats.rxTotal, 2000);
    });

    test('fromMap defaults to zero for missing fields', () {
      final stats = TrafficStats.fromMap({});
      expect(stats.profileId, 0);
      expect(stats.txRate, 0);
      expect(stats.rxRate, 0);
      expect(stats.txTotal, 0);
      expect(stats.rxTotal, 0);
    });

    test('formatBytes formats bytes', () {
      expect(TrafficStats.formatBytes(0), '0 B');
      expect(TrafficStats.formatBytes(512), '512 B');
      expect(TrafficStats.formatBytes(1023), '1023 B');
    });

    test('formatBytes formats kilobytes', () {
      expect(TrafficStats.formatBytes(1024), '1.0 KB');
      expect(TrafficStats.formatBytes(1536), '1.5 KB');
    });

    test('formatBytes formats megabytes', () {
      expect(TrafficStats.formatBytes(1024 * 1024), '1.0 MB');
      expect(TrafficStats.formatBytes(1024 * 1024 * 5), '5.0 MB');
    });

    test('formatBytes formats gigabytes', () {
      expect(TrafficStats.formatBytes(1024 * 1024 * 1024), '1.00 GB');
      expect(TrafficStats.formatBytes(1024 * 1024 * 1024 * 2), '2.00 GB');
    });

    test('formatSpeed appends /s', () {
      expect(TrafficStats.formatSpeed(0), '0 B/s');
      expect(TrafficStats.formatSpeed(1024), '1.0 KB/s');
      expect(TrafficStats.formatSpeed(1024 * 1024), '1.0 MB/s');
    });
  });
}
