# GateKeeper Flutter

Flutter version of the GateKeeper Remote Granular Access Control system.

## Features

- Multi-scope file exploration (Documents, Downloads, Desktop, Pictures, Videos)
- Real-time file lock/unlock controls
- Hierarchical folder navigation
- File search across all scopes
- Service status monitoring
- Dark/Light theme switching
- Real-time updates via Supabase

## Prerequisites

- Flutter SDK 3.0.0 or higher
- Dart SDK
- Supabase account (already configured)

## Installation

1. Install Flutter dependencies:
```bash
cd flutter_gac
flutter pub get
```

2. Run the app:

**Desktop (Windows/macOS/Linux):**
```bash
flutter run -d windows
# or
flutter run -d macos
# or
flutter run -d linux
```

**Web:**
```bash
flutter run -d chrome
```

**Mobile:**
```bash
flutter run -d android
# or
flutter run -d ios
```

## Project Structure

```
flutter_gac/
├── lib/
│   ├── main.dart                  # App entry point
│   ├── models/
│   │   ├── file_item.dart         # File data model
│   │   └── scope.dart             # Scope definitions
│   ├── providers/
│   │   └── theme_provider.dart    # Theme management
│   ├── screens/
│   │   └── home_screen.dart       # Main screen
│   ├── services/
│   │   └── supabase_service.dart  # Supabase integration
│   └── widgets/
│       └── file_explorer.dart     # File explorer component
└── pubspec.yaml                   # Dependencies
```

## Configuration

The app is pre-configured with your Supabase credentials in `lib/main.dart`. No additional configuration needed.

## Building for Production

**Windows:**
```bash
flutter build windows
```

**macOS:**
```bash
flutter build macos
```

**Linux:**
```bash
flutter build linux
```

**Android:**
```bash
flutter build apk
```

**iOS:**
```bash
flutter build ios
```

**Web:**
```bash
flutter build web
```

## Features Comparison with React Version

All features from the React frontend have been ported:

- ✅ Multi-scope navigation
- ✅ File/folder tree view
- ✅ Lock/unlock functionality
- ✅ Search functionality
- ✅ Real-time updates
- ✅ Service status indicator
- ✅ Theme switching
- ✅ Responsive design

## Notes

- The app uses polling (every 3 seconds) to refresh data
- Real-time subscriptions are configured for Supabase
- All paths are Windows-formatted (C:/Users/...)
- The app supports desktop, web, and mobile platforms
