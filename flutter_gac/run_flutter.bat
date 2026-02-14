@echo off
echo Starting GateKeeper Flutter App...
cd /d "%~dp0"

where flutter >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: Flutter is not installed or not in PATH.
    echo Please install Flutter from: https://flutter.dev/docs/get-started/install
    pause
    exit /b 1
)

echo Installing dependencies...
call flutter pub get

if %errorlevel% neq 0 (
    echo ERROR: Failed to install dependencies.
    pause
    exit /b 1
)

echo.
echo Select platform:
echo 1. Windows (Desktop)
echo 2. Web (Chrome)
echo 3. Android
echo.
set /p choice="Enter choice (1-3): "

if "%choice%"=="1" (
    echo Running on Windows...
    flutter run -d windows
) else if "%choice%"=="2" (
    echo Running on Web...
    flutter run -d chrome
) else if "%choice%"=="3" (
    echo Running on Android...
    flutter run -d android
) else (
    echo Invalid choice. Running on Windows by default...
    flutter run -d windows
)
