class Profile {
  final int id;
  final String name;
  final String host;
  final int remotePort;
  final String password;
  final String method;
  final String route;
  final String remoteDns;
  final bool proxyApps;
  final bool bypass;
  final bool udpdns;
  final bool ipv6;
  final bool metered;
  final String individual;
  final String? plugin;
  final int? udpFallback;
  final int subscription;
  final int tx;
  final int rx;
  final int userOrder;

  const Profile({
    required this.id,
    this.name = '',
    required this.host,
    this.remotePort = 8388,
    required this.password,
    this.method = 'chacha20-ietf-poly1305',
    this.route = 'all',
    this.remoteDns = 'dns.google',
    this.proxyApps = false,
    this.bypass = false,
    this.udpdns = false,
    this.ipv6 = false,
    this.metered = false,
    this.individual = '',
    this.plugin,
    this.udpFallback,
    this.subscription = 0,
    this.tx = 0,
    this.rx = 0,
    this.userOrder = 0,
  });

  factory Profile.fromMap(Map<dynamic, dynamic> map) {
    return Profile(
      id: map['id'] as int,
      name: (map['name'] as String?) ?? '',
      host: (map['host'] as String?) ?? '',
      remotePort: (map['remotePort'] as int?) ?? 8388,
      password: (map['password'] as String?) ?? '',
      method: (map['method'] as String?) ?? 'chacha20-ietf-poly1305',
      route: (map['route'] as String?) ?? 'all',
      remoteDns: (map['remoteDns'] as String?) ?? 'dns.google',
      proxyApps: (map['proxyApps'] as bool?) ?? false,
      bypass: (map['bypass'] as bool?) ?? false,
      udpdns: (map['udpdns'] as bool?) ?? false,
      ipv6: (map['ipv6'] as bool?) ?? false,
      metered: (map['metered'] as bool?) ?? false,
      individual: (map['individual'] as String?) ?? '',
      plugin: map['plugin'] as String?,
      udpFallback: map['udpFallback'] as int?,
      subscription: (map['subscription'] as int?) ?? 0,
      tx: (map['tx'] as int?) ?? 0,
      rx: (map['rx'] as int?) ?? 0,
      userOrder: (map['userOrder'] as int?) ?? 0,
    );
  }

  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'name': name,
      'host': host,
      'remotePort': remotePort,
      'password': password,
      'method': method,
      'route': route,
      'remoteDns': remoteDns,
      'proxyApps': proxyApps,
      'bypass': bypass,
      'udpdns': udpdns,
      'ipv6': ipv6,
      'metered': metered,
      'individual': individual,
      'plugin': plugin,
      'udpFallback': udpFallback,
      'subscription': subscription,
      'tx': tx,
      'rx': rx,
      'userOrder': userOrder,
    };
  }

  Profile copyWith({
    int? id,
    String? name,
    String? host,
    int? remotePort,
    String? password,
    String? method,
    String? route,
    String? remoteDns,
    bool? proxyApps,
    bool? bypass,
    bool? udpdns,
    bool? ipv6,
    bool? metered,
    String? individual,
    String? plugin,
    int? udpFallback,
    int? subscription,
    int? tx,
    int? rx,
    int? userOrder,
  }) {
    return Profile(
      id: id ?? this.id,
      name: name ?? this.name,
      host: host ?? this.host,
      remotePort: remotePort ?? this.remotePort,
      password: password ?? this.password,
      method: method ?? this.method,
      route: route ?? this.route,
      remoteDns: remoteDns ?? this.remoteDns,
      proxyApps: proxyApps ?? this.proxyApps,
      bypass: bypass ?? this.bypass,
      udpdns: udpdns ?? this.udpdns,
      ipv6: ipv6 ?? this.ipv6,
      metered: metered ?? this.metered,
      individual: individual ?? this.individual,
      plugin: plugin ?? this.plugin,
      udpFallback: udpFallback ?? this.udpFallback,
      subscription: subscription ?? this.subscription,
      tx: tx ?? this.tx,
      rx: rx ?? this.rx,
      userOrder: userOrder ?? this.userOrder,
    );
  }

  String get displayName => name.isNotEmpty ? name : '$host:$remotePort';

  String get methodDisplay => method;

  static const encryptionMethods = [
    'chacha20-ietf-poly1305',
    'aes-256-gcm',
    'aes-128-gcm',
    '2022-blake3-aes-256-gcm',
    '2022-blake3-aes-128-gcm',
    '2022-blake3-chacha20-poly1305',
    'xchacha20-ietf-poly1305',
    'aes-256-cfb',
    'aes-192-cfb',
    'aes-128-cfb',
    'chacha20-ietf',
    'rc4-md5',
    'none',
  ];

  static const routeOptions = {
    'all': 'All',
    'bypass-lan': 'Bypass LAN',
    'bypass-china': 'Bypass China',
    'bypass-lan-china': 'Bypass LAN & China',
    'gfwlist': 'GFW List',
    'china-list': 'China List',
    'custom-rules': 'Custom Rules',
  };
}
