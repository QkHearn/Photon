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

:: 4. 读取 config.json 中的 Tree-sitter 开关
set TS_FLAG=OFF
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "(Get-Content config.json | ConvertFrom-Json).agent.enable_tree_sitter"`) do set TS_FLAG=%%i
if /I "%TS_FLAG%"=="True" (
    set TS_DEFINE=/D PHOTON_ENABLE_TREESITTER
    set TS_INCLUDES=/I third_party\tree-sitter\lib\include /I third_party\tree-sitter-cpp\bindings\c /I third_party\tree-sitter-python\bindings\c /I third_party\tree-sitter-typescript\bindings\c /I third_party\tree-sitter-arkts\bindings\c
    if not exist "third_party\tree-sitter" (
        echo [Error] Tree-sitter not found in third_party.
        exit /b 1
    )
    if not exist "third_party\tree-sitter-cpp" (
        echo [Error] tree-sitter-cpp not found in third_party.
        exit /b 1
    )
    if not exist "third_party\tree-sitter-python" (
        echo [Error] tree-sitter-python not found in third_party.
        exit /b 1
    )
    if not exist "third_party\tree-sitter-typescript" (
        echo [Error] tree-sitter-typescript not found in third_party.
        exit /b 1
    )
    if not exist "third_party\tree-sitter-arkts" (
        echo [Error] tree-sitter-arkts not found in third_party.
        exit /b 1
    )
    if not exist "build\ts" mkdir build\ts
    echo [Photon] Compiling Tree-sitter C sources...
    cl /nologo /TC /c third_party\tree-sitter\lib\src\lib.c %TS_INCLUDES% /Fo build\ts\tree-sitter_lib_src_lib.obj
    cl /nologo /TC /c third_party\tree-sitter-cpp\src\parser.c %TS_INCLUDES% /Fo build\ts\tree-sitter-cpp_src_parser.obj
    cl /nologo /TC /c third_party\tree-sitter-cpp\src\scanner.c %TS_INCLUDES% /Fo build\ts\tree-sitter-cpp_src_scanner.obj
    cl /nologo /TC /c third_party\tree-sitter-python\src\parser.c %TS_INCLUDES% /Fo build\ts\tree-sitter-python_src_parser.obj
    cl /nologo /TC /c third_party\tree-sitter-python\src\scanner.c %TS_INCLUDES% /Fo build\ts\tree-sitter-python_src_scanner.obj
    cl /nologo /TC /c third_party\tree-sitter-typescript\typescript\src\parser.c %TS_INCLUDES% /I third_party\tree-sitter-typescript\common /I third_party\tree-sitter-typescript\typescript\src /Fo build\ts\tree-sitter-typescript_src_parser.obj
    cl /nologo /TC /c third_party\tree-sitter-typescript\typescript\src\scanner.c %TS_INCLUDES% /I third_party\tree-sitter-typescript\common /I third_party\tree-sitter-typescript\typescript\src /Fo build\ts\tree-sitter-typescript_src_scanner.obj
    cl /nologo /TC /c third_party\tree-sitter-arkts\src\parser.c %TS_INCLUDES% /Fo build\ts\tree-sitter-arkts_src_parser.obj
    set TS_OBJS=build\ts\tree-sitter_lib_src_lib.obj build\ts\tree-sitter-cpp_src_parser.obj build\ts\tree-sitter-cpp_src_scanner.obj build\ts\tree-sitter-python_src_parser.obj build\ts\tree-sitter-python_src_scanner.obj build\ts\tree-sitter-typescript_src_parser.obj build\ts\tree-sitter-typescript_src_scanner.obj build\ts\tree-sitter-arkts_src_parser.obj
) else (
    set TS_DEFINE=
    set TS_INCLUDES=
    set TS_OBJS=
)

:: 5. 执行编译
echo [Photon] Compiling...
cl /EHsc /O2 /std:c++17 ^
   src\core\main.cpp src\utils\FileManager.cpp src\utils\SymbolManager.cpp src\utils\RegexSymbolProvider.cpp src\utils\TreeSitterSymbolProvider.cpp src\core\LLMClient.cpp src\core\ContextManager.cpp src\mcp\MCPClient.cpp src\mcp\LSPClient.cpp src\mcp\InternalMCPClient.cpp ^
   /I src /I "%VCPKG_INC%" %TS_INCLUDES% %TS_DEFINE% ^
   %TS_OBJS% ^
   /link /LIBPATH:"%VCPKG_LIB%" libssl.lib libcrypto.lib ws2_32.lib crypt32.lib advapi32.lib user32.lib ^
   /out:photon.exe

if %errorlevel% equ 0 (
    echo [Photon] Build Successful! Output: photon.exe
) else (
    echo [Photon] Build Failed.
    exit /b 1
)
