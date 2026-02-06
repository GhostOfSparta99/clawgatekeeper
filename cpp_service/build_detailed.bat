@echo off
echo Building GateKeeperService with detailed output...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" 2>&1
cd /d "%~dp0"
echo.
echo Current directory: %CD%
echo.
echo Checking include paths...
if exist "..\..\GateKeeper-main\vcpkg\installed\x64-windows\include" (
    echo Include path exists: YES
) else (
    echo Include path exists: NO
    echo Path checked: ..\..\GateKeeper-main\vcpkg\installed\x64-windows\include
)
echo.
echo Checking lib paths...
if exist "..\..\GateKeeper-main\vcpkg\installed\x64-windows\lib" (
    echo Lib path exists: YES
) else (
    echo Lib path exists: NO
    echo Path checked: ..\..\GateKeeper-main\vcpkg\installed\x64-windows\lib
)
echo.
echo Starting compilation...
cl /std:c++17 /EHsc /DJSON_DLL /Fe:GateKeeperConsoleLocal_v2.exe GateKeeperService.cpp /I "..\..\GateKeeper-main\vcpkg\installed\x64-windows\include" /link /LIBPATH:"..\..\GateKeeper-main\vcpkg\installed\x64-windows\lib" libcurl.lib jsoncpp.lib Advapi32.lib 2>&1
echo.
echo Build exit code: %errorlevel%
if %errorlevel% equ 0 (
    echo Build successful!
    echo Executable: GateKeeperConsoleLocal_v2.exe
) else (
    echo Build failed with error code %errorlevel%
)
pause
