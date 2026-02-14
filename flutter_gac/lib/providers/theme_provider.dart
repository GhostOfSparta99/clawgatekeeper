import 'package:flutter/material.dart';

class ThemeProvider extends ChangeNotifier {
  bool _isStealthMode = true;

  bool get isStealthMode => _isStealthMode;

  ThemeData get currentTheme => _isStealthMode ? stealthTheme : dayTheme;

  void toggleTheme() {
    _isStealthMode = !_isStealthMode;
    notifyListeners();
  }

  static final ThemeData stealthTheme = ThemeData(
    brightness: Brightness.dark,
    scaffoldBackgroundColor: const Color(0xFF030712),
    primaryColor: const Color(0xFF3B82F6),
    cardColor: const Color(0xFF111827),
    dividerColor: Colors.white10,
    textTheme: const TextTheme(
      bodyLarge: TextStyle(color: Color(0xFFE5E7EB)),
      bodyMedium: TextStyle(color: Color(0xFFE5E7EB)),
      titleLarge: TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
    ),
    iconTheme: const IconThemeData(color: Color(0xFF9CA3AF)),
    appBarTheme: const AppBarTheme(
      backgroundColor: Color(0xFF111827),
      foregroundColor: Colors.white,
      elevation: 0,
    ),
  );

  static final ThemeData dayTheme = ThemeData(
    brightness: Brightness.light,
    scaffoldBackgroundColor: const Color(0xFFF9FAFB),
    primaryColor: const Color(0xFF3B82F6),
    cardColor: Colors.white,
    dividerColor: const Color(0xFFE5E7EB),
    textTheme: const TextTheme(
      bodyLarge: TextStyle(color: Color(0xFF111827)),
      bodyMedium: TextStyle(color: Color(0xFF111827)),
      titleLarge: TextStyle(color: Color(0xFF111827), fontWeight: FontWeight.bold),
    ),
    iconTheme: const IconThemeData(color: Color(0xFF6B7280)),
    appBarTheme: const AppBarTheme(
      backgroundColor: Colors.white,
      foregroundColor: Color(0xFF111827),
      elevation: 0,
    ),
  );
}
