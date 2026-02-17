enum ServiceState {
  idle(0),
  connecting(1),
  connected(2),
  stopping(3),
  stopped(4);

  final int value;
  const ServiceState(this.value);

  factory ServiceState.fromValue(int value) {
    return ServiceState.values.firstWhere(
      (s) => s.value == value,
      orElse: () => ServiceState.idle,
    );
  }

  bool get isStarted => this == connected;
  bool get isBusy => this == connecting || this == stopping;
  bool get canToggle => !isBusy;
}

class ServiceStatus {
  final ServiceState state;
  final String? profileName;
  final String? message;

  const ServiceStatus({
    this.state = ServiceState.idle,
    this.profileName,
    this.message,
  });

  factory ServiceStatus.fromMap(Map<dynamic, dynamic> map) {
    return ServiceStatus(
      state: ServiceState.fromValue(map['state'] as int? ?? 0),
      profileName: map['profileName'] as String?,
      message: map['message'] as String?,
    );
  }
}
