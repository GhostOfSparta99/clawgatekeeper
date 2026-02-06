@echo off
setlocal

echo [INFO] Setting up Visual Studio Environment...
pushd %~dp0
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if %errorlevel% neq 0 (
    echo [ERROR] Failed to setup environment.
    exit /b %errorlevel%
)

echo [INFO] Compiling GateKeeperService...
cl /std:c++17 /EHsc /DJSON_DLL GateKeeperService.cpp /I "..\..\GateKeeper-main\vcpkg\installed\x64-windows\include" /link /LIBPATH:"..\..\GateKeeper-main\vcpkg\installed\x64-windows\lib" libcurl.lib jsoncpp.lib Advapi32.lib

if %errorlevel% equ 0 (
    echo [SUCCESS] Build completed. GateKeeperService.exe created.
    echo [INFO] Copying DLLs...
    copy /Y "..\..\GateKeeper-main\vcpkg\installed\x64-windows\bin\*.dll" .
) else (
    echo [ERROR] Compilation failed.
)
