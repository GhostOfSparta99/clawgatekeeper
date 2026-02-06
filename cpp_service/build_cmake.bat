@echo off
setlocal
echo [INFO] Building with CMake...

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake is not found in PATH. Please install CMake.
    exit /b 1
)

if not exist build mkdir build
cd build

echo [INFO] Configuring...
REM Ideally point to vcpkg toolchain if available
REM cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake ..

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %errorlevel%
)

echo [INFO] Building...
cmake --build . --config Release

if %errorlevel% equ 0 (
    echo [SUCCESS] Build completed.
    echo Executable is in cpp_service\build\Release\GateKeeperService.exe
) else (
    echo [ERROR] Build failed.
)
