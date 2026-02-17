import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'screens/about/about_screen.dart';
import 'screens/app_manager/app_manager_screen.dart';
import 'screens/custom_rules/custom_rules_screen.dart';
import 'screens/home/home_screen.dart';
import 'screens/profile_config/profile_config_screen.dart';
import 'screens/scanner/scanner_screen.dart';
import 'screens/settings/settings_screen.dart';
import 'screens/subscriptions/subscriptions_screen.dart';
import 'theme/app_theme.dart';

final _router = GoRouter(
  routes: [
    GoRoute(
      path: '/',
      builder: (context, state) => const HomeScreen(),
    ),
    GoRoute(
      path: '/profile/new',
      builder: (context, state) => const ProfileConfigScreen(),
    ),
    GoRoute(
      path: '/profile/:id',
      builder: (context, state) => ProfileConfigScreen(
        profileId: state.pathParameters['id'],
      ),
    ),
    GoRoute(
      path: '/settings',
      builder: (context, state) => const SettingsScreen(),
    ),
    GoRoute(
      path: '/subscriptions',
      builder: (context, state) => const SubscriptionsScreen(),
    ),
    GoRoute(
      path: '/custom-rules',
      builder: (context, state) => const CustomRulesScreen(),
    ),
    GoRoute(
      path: '/scanner',
      builder: (context, state) => const ScannerScreen(),
    ),
    GoRoute(
      path: '/app-manager',
      builder: (context, state) => const AppManagerScreen(),
    ),
    GoRoute(
      path: '/about',
      builder: (context, state) => const AboutScreen(),
    ),
  ],
);

class ShadowsocksApp extends StatelessWidget {
  const ShadowsocksApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp.router(
      title: 'Shadowsocks',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light,
      darkTheme: AppTheme.dark,
      themeMode: ThemeMode.system,
      routerConfig: _router,
    );
  }
}
