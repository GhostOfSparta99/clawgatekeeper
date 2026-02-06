# GateKeeper C++ Service

This is the native C++ implementation of the GateKeeper service. It runs as a Windows Service, monitors the `Documents` folder, and enforces permissions based on Supabase state.

## Prerequisites

- **Visual Studio 2022 Community** (Installed at `C:\Program Files\Microsoft Visual Studio\18\Community`)
- **vcpkg** (Installed at `C:\Users\Adi\Desktop\GateKeeper-main\vcpkg`)

## Compilation

### 1. Initialize Build Environment
Open PowerShell and run the following command to set up the MSVC environment variables:
```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### 2. Compile the Service
Run this command from the `cpp_service` directory (or adjust paths accordingly):
```powershell
cl /std:c++17 /EHsc /DJSON_DLL GateKeeperService.cpp /I ..\vcpkg\installed\x64-windows\include /link /LIBPATH:..\vcpkg\installed\x64-windows\lib libcurl.lib jsoncpp.lib Advapi32.lib
```

> **Note**: If you are in the root `GateKeeper-main` folder, use:
> ```powershell
> cl /std:c++17 /EHsc /DJSON_DLL cpp_service\GateKeeperService.cpp /I vcpkg\installed\x64-windows\include /link /LIBPATH:vcpkg\installed\x64-windows\lib libcurl.lib jsoncpp.lib Advapi32.lib /out:cpp_service\GateKeeperService.exe
> ```

## Installation

Run PowerShell as **Administrator**:

1. **Create the Service**
   ```powershell
   New-Service -Name "GateKeeperService" -BinaryPathName "C:\Users\Adi\Desktop\GateKeeper-main\cpp_service\GateKeeperService.exe" -Description "GateKeeper Cloud File Permissions" -StartupType Automatic
   ```

2. **Start the Service**
   ```powershell
   Start-Service GateKeeperService
   ```

3. **Check Status**
   ```powershell
   Get-Service GateKeeperService
   ```

## Uninstall
```powershell
Stop-Service GateKeeperService
Remove-Service GateKeeperService
```
