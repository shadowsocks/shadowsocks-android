class TrafficStats {
  final int profileId;
  final int txRate;
  final int rxRate;
  final int txTotal;
  final int rxTotal;

  const TrafficStats({
    this.profileId = 0,
    this.txRate = 0,
    this.rxRate = 0,
    this.txTotal = 0,
    this.rxTotal = 0,
  });

  factory TrafficStats.fromMap(Map<dynamic, dynamic> map) {
    return TrafficStats(
      profileId: (map['profileId'] as int?) ?? 0,
      txRate: (map['txRate'] as int?) ?? 0,
      rxRate: (map['rxRate'] as int?) ?? 0,
      txTotal: (map['txTotal'] as int?) ?? 0,
      rxTotal: (map['rxTotal'] as int?) ?? 0,
    );
  }

  static String formatBytes(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    if (bytes < 1024 * 1024 * 1024) {
      return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
    }
    return '${(bytes / (1024 * 1024 * 1024)).toStringAsFixed(2)} GB';
  }

  static String formatSpeed(int bytesPerSec) {
    return '${formatBytes(bytesPerSec)}/s';
  }
}
