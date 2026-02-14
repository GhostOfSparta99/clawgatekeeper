#!/bin/bash

echo "Starting GateKeeper Flutter App..."
cd "$(dirname "$0")"

if ! command -v flutter &> /dev/null; then
    echo "ERROR: Flutter is not installed or not in PATH."
    echo "Please install Flutter from: https://flutter.dev/docs/get-started/install"
    exit 1
fi

echo "Installing dependencies..."
flutter pub get

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install dependencies."
    exit 1
fi

echo ""
echo "Select platform:"
echo "1. Linux (Desktop)"
echo "2. macOS (Desktop)"
echo "3. Web (Chrome)"
echo "4. Android"
echo "5. iOS"
echo ""
read -p "Enter choice (1-5): " choice

case $choice in
    1)
        echo "Running on Linux..."
        flutter run -d linux
        ;;
    2)
        echo "Running on macOS..."
        flutter run -d macos
        ;;
    3)
        echo "Running on Web..."
        flutter run -d chrome
        ;;
    4)
        echo "Running on Android..."
        flutter run -d android
        ;;
    5)
        echo "Running on iOS..."
        flutter run -d ios
        ;;
    *)
        echo "Invalid choice. Running on Linux by default..."
        flutter run -d linux
        ;;
esac
