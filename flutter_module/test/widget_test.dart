import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_module/app.dart';

void main() {
  testWidgets('App renders', (WidgetTester tester) async {
    await tester.pumpWidget(
      const ProviderScope(child: ShadowsocksApp()),
    );
    expect(find.text('Shadowsocks'), findsWidgets);
  });
}
