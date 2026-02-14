@echo off
setlocal
echo [INFO] Setting up Visual Studio Environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
echo [INFO] Compiling separate Console Executable...
pushd %~dp0
cl /std:c++17 /EHsc /DJSON_DLL /Fe:GateKeeperConsoleLocal_v2.exe GateKeeperService.cpp /I "C:\Users\Adi\Desktop\GateKeeper-main\vcpkg\installed\x64-windows\include" /link /LIBPATH:"C:\Users\Adi\Desktop\GateKeeper-main\vcpkg\installed\x64-windows\lib" libcurl.lib jsoncpp.lib Advapi32.lib
if %errorlevel% equ 0 (
    echo [INFO] Copying DLLs...
    copy /Y "C:\Users\Adi\Desktop\GateKeeper-main\vcpkg\installed\x64-windows\bin\*.dll" .
    echo [SUCCESS] GateKeeperConsoleLocal_v2.exe created.
)
