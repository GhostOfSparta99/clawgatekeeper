@echo off
echo Building GateKeeperService...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl /std:c++17 /EHsc /DJSON_DLL /Fe:GateKeeperService.exe GateKeeperService.cpp /I "C:\Users\Adi\Desktop\GateKeeper-main\vcpkg\installed\x64-windows\include" /link /LIBPATH:"C:\Users\Adi\Desktop\GateKeeper-main\vcpkg\installed\x64-windows\lib" libcurl.lib jsoncpp.lib Advapi32.lib
if %errorlevel% equ 0 (
    echo Build successful!
    echo Executable: GateKeeperConsoleLocal_v2.exe
) else (
    echo Build failed with error code %errorlevel%
)
pause
