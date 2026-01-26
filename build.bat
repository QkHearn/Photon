@echo off
setlocal enabledelayedexpansion

echo [Photon] Starting Windows Build (MSVC)...

:: 1. 检查 vcpkg 环境变量
if "%VCPKG_ROOT%"=="" (
    echo [Error] VCPKG_ROOT environment variable is not set.
    echo Please install vcpkg and set VCPKG_ROOT, or edit this script to point to your vcpkg path.
    exit /b 1
)

:: 2. 检查依赖项 (通过 vcpkg 路径检查)
set VCPKG_INC=%VCPKG_ROOT%\installed\x64-windows\include
set VCPKG_LIB=%VCPKG_ROOT%\installed\x64-windows\lib

if not exist "%VCPKG_INC%\nlohmann\json.hpp" (
    echo [Error] nlohmann-json not found in vcpkg.
    echo Run: vcpkg install nlohmann-json:x64-windows
    exit /b 1
)

if not exist "%VCPKG_INC%\openssl\ssl.h" (
    echo [Error] OpenSSL not found in vcpkg.
    echo Run: vcpkg install openssl:x64-windows
    exit /b 1
)

:: 3. 查找编译器 (MSVC)
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [Error] MSVC compiler (cl.exe) not found. 
    echo Please run this script from the 'Developer Command Prompt for VS'.
    exit /b 1
)

:: 4. 执行编译
echo [Photon] Compiling...
cl /EHsc /O2 /std:c++17 ^
   src\core\main.cpp src\utils\FileManager.cpp src\core\LLMClient.cpp src\core\ContextManager.cpp src\mcp\MCPClient.cpp src\mcp\InternalMCPClient.cpp ^
   /I src /I "%VCPKG_INC%" ^
   /link /LIBPATH:"%VCPKG_LIB%" libssl.lib libcrypto.lib ws2_32.lib crypt32.lib advapi32.lib user32.lib ^
   /out:photon.exe

if %errorlevel% equ 0 (
    echo [Photon] Build Successful! Output: photon.exe
) else (
    echo [Photon] Build Failed.
    exit /b 1
)
