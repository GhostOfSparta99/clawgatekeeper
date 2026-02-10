# Quick Start Guide

## Installation

### 1. Install Flutter

**Windows:**
1. Download Flutter SDK from https://flutter.dev/docs/get-started/install/windows
2. Extract to `C:\flutter`
3. Add to PATH: `C:\flutter\bin`

**macOS:**
```bash
brew install flutter
```

**Linux:**
```bash
sudo snap install flutter --classic
```

### 2. Verify Installation
```bash
flutter doctor
```

This checks for:
- Flutter SDK
- Android Studio (optional, for Android development)
- VS Code or Android Studio plugins
- Connected devices

## Running the App

### Quick Run (Windows)
```bash
cd flutter_gac
run_flutter.bat
```

### Quick Run (macOS/Linux)
```bash
cd flutter_gac
./run_flutter.sh
```

### Manual Run

**Desktop:**
```bash
flutter pub get
flutter run -d windows    # Windows
flutter run -d macos      # macOS
flutter run -d linux      # Linux
```

**Web:**
```bash
flutter run -d chrome
```

**Mobile:**
```bash
flutter run -d android
flutter run -d ios
```

## Building for Production

### Windows Desktop
```bash
flutter build windows --release
# Output: build/windows/x64/runner/Release/
```

### Web
```bash
flutter build web --release
# Output: build/web/
```

### Android APK
```bash
flutter build apk --release
# Output: build/app/outputs/flutter-apk/app-release.apk
```

## Project Structure

```
flutter_gac/
├── lib/
│   ├── main.dart                 # Entry point
│   ├── models/                   # Data models
│   ├── providers/                # State management
│   ├── screens/                  # Main screens
│   ├── services/                 # API services
│   └── widgets/                  # Reusable widgets
├── pubspec.yaml                  # Dependencies
├── README.md                     # Documentation
└── MIGRATION_GUIDE.md            # React → Flutter guide
```

## Configuration

All Supabase credentials are pre-configured in `lib/main.dart`:
- URL: `https://hyqmdnzkxhkvzynzqwug.supabase.co`
- Key: Already embedded

No additional configuration needed.

## Common Commands

```bash
# Get dependencies
flutter pub get

# Run in debug mode
flutter run

# Run with hot reload
flutter run --hot

# Build release version
flutter build <platform> --release

# Clean build cache
flutter clean

# Update Flutter
flutter upgrade

# Check for issues
flutter doctor -v
```

## Troubleshooting

### Issue: "Flutter command not found"
**Solution:** Add Flutter to your PATH

### Issue: "No devices found"
**Solution:**
- For desktop: Run `flutter config --enable-windows-desktop`
- For web: Run `flutter config --enable-web`

### Issue: "Pub get failed"
**Solution:** Delete `pubspec.lock` and run `flutter pub get` again

### Issue: "Build failed"
**Solution:** Run `flutter clean` then rebuild

## Development Tips

1. **Hot Reload**: Press `r` in the terminal while app is running
2. **Hot Restart**: Press `R` for full restart
3. **Widget Inspector**: Press `w` to toggle widget inspector
4. **Performance Overlay**: Press `p` to show performance metrics

## IDE Setup

### VS Code
1. Install "Flutter" extension
2. Install "Dart" extension
3. Open flutter_gac folder
4. Press F5 to run

### Android Studio
1. Install Flutter plugin
2. Install Dart plugin
3. Open flutter_gac folder
4. Click Run button

## Next Steps

1. Run `flutter doctor` to verify setup
2. Navigate to `flutter_gac` directory
3. Run `flutter pub get`
4. Choose your platform and run!

For more details, see [README.md](README.md) and [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md).
