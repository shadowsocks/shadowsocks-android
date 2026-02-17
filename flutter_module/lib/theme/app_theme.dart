import 'package:flutter/material.dart';

class AppTheme {
  // Brand colors - Blue Grey palette with Green accent
  static const _primaryBlueGrey = Color(0xFF607D8B);
  static const _accentGreen = Color(0xFF00C853);

  static final _lightColorScheme = ColorScheme.fromSeed(
    seedColor: _primaryBlueGrey,
    primary: _primaryBlueGrey,
    secondary: _accentGreen,
    brightness: Brightness.light,
  );

  static final _darkColorScheme = ColorScheme.fromSeed(
    seedColor: _primaryBlueGrey,
    primary: const Color(0xFF90A4AE),
    secondary: _accentGreen,
    surface: const Color(0xFF1A1C1E),
    brightness: Brightness.dark,
  );

  static ThemeData get light => _buildTheme(_lightColorScheme);
  static ThemeData get dark => _buildTheme(_darkColorScheme);

  static ThemeData _buildTheme(ColorScheme colorScheme) {
    final textTheme = ThemeData(colorScheme: colorScheme).textTheme;

    return ThemeData(
      useMaterial3: true,
      colorScheme: colorScheme,
      textTheme: textTheme,
      appBarTheme: AppBarTheme(
        centerTitle: false,
        elevation: 0,
        scrolledUnderElevation: 2,
        backgroundColor: colorScheme.surface,
        foregroundColor: colorScheme.onSurface,
        titleTextStyle: textTheme.titleLarge?.copyWith(
          color: colorScheme.onSurface,
          fontWeight: FontWeight.w600,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: BorderSide(color: colorScheme.outlineVariant.withAlpha(128)),
        ),
        clipBehavior: Clip.antiAlias,
      ),
      floatingActionButtonTheme: FloatingActionButtonThemeData(
        elevation: 2,
        highlightElevation: 4,
        backgroundColor: colorScheme.secondary,
        foregroundColor: Colors.white,
        shape: const CircleBorder(),
        largeSizeConstraints: const BoxConstraints.tightFor(
          width: 64,
          height: 64,
        ),
      ),
      navigationDrawerTheme: NavigationDrawerThemeData(
        indicatorShape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(28),
        ),
      ),
      listTileTheme: ListTileThemeData(
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: colorScheme.surfaceContainerHighest.withAlpha(77),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide.none,
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: colorScheme.primary, width: 2),
        ),
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      ),
      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
      dialogTheme: DialogThemeData(
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(28)),
      ),
      bottomSheetTheme: const BottomSheetThemeData(
        showDragHandle: true,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(28)),
        ),
      ),
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return colorScheme.secondary;
          }
          return null;
        }),
        trackColor: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return colorScheme.secondary.withAlpha(77);
          }
          return null;
        }),
      ),
      dividerTheme: DividerThemeData(
        color: colorScheme.outlineVariant.withAlpha(77),
        space: 1,
      ),
    );
  }
}
