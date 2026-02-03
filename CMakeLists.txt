cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(Photon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Code coverage option
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Code coverage is only supported with GCC or Clang")
    endif()
endif()

# Windows specific global settings
if(MSVC)
    # Force static runtime /MT for all targets to match vcpkg's x64-windows-static
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Alternative for older CMake or if the above is ignored
    add_compile_options(
        $<$<NOT:$<CONFIG:Debug>>:/MT>
        $<$<CONFIG:Debug>:/MTd>
    )
endif()

if(WIN32)
    # Force OpenSSL to use static libs
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()

# Find dependencies
# Note: You need to have nlohmann_json and cpp-httplib installed

# On macOS with Homebrew, we often need to explicitly add the include path
if(APPLE)
    set(PHOTON_BREW_INCLUDE "/opt/homebrew/include")
    set(PHOTON_BREW_LIB "/opt/homebrew/lib")
endif()

# --- FTXUI Integration ---
add_subdirectory(third_party/ftxui)

# nlohmann_json (use bundled on Apple to avoid mixed include paths)
option(PHOTON_USE_BUNDLED_JSON "Build nlohmann_json from source" ${APPLE})
if(PHOTON_USE_BUNDLED_JSON)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # 设置为使用多头文件版本
    set(JSON_MultipleHeaders ON CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
else()
    find_package(nlohmann_json REQUIRED)
endif()
# Optional SQLite3 for semantic index storage
find_package(SQLite3)
# cpp-httplib is often header-only, if not found via find_package, 
# we expect it to be in the include path.

add_executable(photon src/core/main.cpp)

# Add src to include path
target_include_directories(photon PRIVATE src third_party/httplib)
if(APPLE)
    target_include_directories(photon PRIVATE ${PHOTON_BREW_INCLUDE})
    target_link_directories(photon PRIVATE ${PHOTON_BREW_LIB})
endif()

target_link_libraries(photon PRIVATE 
    agent_lib
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
    ftxui::component
    OpenSSL::SSL 
    OpenSSL::Crypto
    Threads::Threads
)

# --- Unit Tests ---
enable_testing()

# Create a library for shared logic to be tested
add_library(agent_lib 
    # Core infrastructure
    src/utils/Logger.cpp
    src/core/UIManager.cpp
    src/core/LLMClient.cpp 
    src/core/ContextManager.cpp
    src/mcp/MCPClient.cpp
    # Agent layer (NEW)
    src/agent/AgentRuntime.cpp
    src/agent/EnvironmentDetector.cpp
    # Tools layer (NEW)
    src/tools/ToolRegistry.cpp
    src/tools/CoreTools.cpp
    src/tools/ViewSymbolTool.cpp
    # Memory layer (NEW)
    src/memory/MemoryManager.cpp
    src/memory/ProjectMemory.cpp
    src/memory/FailureMemory.cpp
    # Analysis layer (REFACTORED)
    src/analysis/SymbolManager.cpp
    src/analysis/LogicMapper.cpp
    src/analysis/SemanticManager.cpp
    src/analysis/LSPClient.cpp
    src/analysis/providers/RegexSymbolProvider.cpp
    src/analysis/providers/TreeSitterSymbolProvider.cpp
)
target_include_directories(agent_lib PUBLIC src third_party/httplib)
if(APPLE)
    target_include_directories(agent_lib PRIVATE ${PHOTON_BREW_INCLUDE})
    target_link_directories(agent_lib PRIVATE ${PHOTON_BREW_LIB})
endif()
target_link_libraries(agent_lib PRIVATE 
    nlohmann_json::nlohmann_json 
    OpenSSL::SSL 
    OpenSSL::Crypto
    ftxui::screen
    ftxui::dom
    ftxui::component
)

if(SQLite3_FOUND)
    target_include_directories(photon PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_include_directories(agent_lib PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(photon PRIVATE ${SQLite3_LIBRARIES})
    target_link_libraries(agent_lib PRIVATE ${SQLite3_LIBRARIES})
    target_compile_definitions(photon PRIVATE PHOTON_USE_SQLITE)
    target_compile_definitions(agent_lib PRIVATE PHOTON_USE_SQLITE)
endif()

# Find or fetch GTest (use bundled on Apple to avoid arch mismatches)
option(PHOTON_USE_BUNDLED_GTEST "Build googletest from source" ${APPLE})
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
if(PHOTON_USE_BUNDLED_GTEST)
    include(FetchContent)
    # 使用更新的 googletest 版本以避免 CMake 兼容性警告
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # 设置 googletest 的 CMake 最低版本要求
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
    FetchContent_MakeAvailable(googletest)
else()
    find_package(GTest REQUIRED)
endif()

add_executable(agent_tests 
    tests/test_ContextManager.cpp
    # TODO: 添加新的测试文件
    # tests/test_ToolRegistry.cpp
    # tests/test_CoreTools.cpp
    # tests/test_MemoryManager.cpp
)
if(PHOTON_USE_BUNDLED_GTEST)
    target_link_libraries(agent_tests PRIVATE gtest gtest_main agent_lib nlohmann_json::nlohmann_json)
else()
    target_link_libraries(agent_tests PRIVATE GTest::gtest GTest::gtest_main agent_lib nlohmann_json::nlohmann_json)
endif()

add_test(NAME AgentTests COMMAND agent_tests)

# On Linux/macOS, we might need OpenSSL for HTTPS
find_package(OpenSSL REQUIRED)

# On Windows, we need to link against ws2_32 and crypt32
if(WIN32)
    target_link_libraries(photon PRIVATE ws2_32 crypt32 bcrypt)
    target_link_libraries(agent_lib PRIVATE ws2_32 crypt32 bcrypt)
    target_link_libraries(agent_tests PRIVATE ws2_32 crypt32 bcrypt)
endif()

# Link pthread if needed
find_package(Threads REQUIRED)

# --- Optional Tree-sitter ---
option(PHOTON_ENABLE_TREESITTER "Enable Tree-sitter parser support" OFF)
if(PHOTON_ENABLE_TREESITTER)
    # Force tree-sitter to build as a static library to avoid DLL issues on Windows
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    add_subdirectory(third_party/tree-sitter)

    add_library(tree-sitter-cpp STATIC
        third_party/tree-sitter-cpp/src/parser.c
        third_party/tree-sitter-cpp/src/scanner.c
    )
    target_include_directories(tree-sitter-cpp PRIVATE third_party/tree-sitter/lib/include)
    target_include_directories(tree-sitter-cpp PUBLIC third_party/tree-sitter-cpp/bindings/c)
    target_link_libraries(tree-sitter-cpp PRIVATE tree-sitter)

    add_library(tree-sitter-python STATIC
        third_party/tree-sitter-python/src/parser.c
        third_party/tree-sitter-python/src/scanner.c
    )
    target_include_directories(tree-sitter-python PRIVATE third_party/tree-sitter/lib/include)
    target_include_directories(tree-sitter-python PUBLIC third_party/tree-sitter-python/bindings/c)
    target_link_libraries(tree-sitter-python PRIVATE tree-sitter)

    add_library(tree-sitter-typescript STATIC
        third_party/tree-sitter-typescript/typescript/src/parser.c
        third_party/tree-sitter-typescript/typescript/src/scanner.c
    )
    target_include_directories(tree-sitter-typescript PRIVATE 
        third_party/tree-sitter/lib/include
        third_party/tree-sitter-typescript/common
        third_party/tree-sitter-typescript/typescript/src
    )
    target_include_directories(tree-sitter-typescript PUBLIC third_party/tree-sitter-typescript/bindings/c)
    target_link_libraries(tree-sitter-typescript PRIVATE tree-sitter)

    add_library(tree-sitter-arkts STATIC
        third_party/tree-sitter-arkts/src/parser.c
    )
    target_include_directories(tree-sitter-arkts PRIVATE third_party/tree-sitter/lib/include)
    target_include_directories(tree-sitter-arkts PUBLIC third_party/tree-sitter-arkts/bindings/c)
    target_link_libraries(tree-sitter-arkts PRIVATE tree-sitter)

    target_link_libraries(photon PRIVATE 
        tree-sitter 
        tree-sitter-cpp 
        tree-sitter-python 
        tree-sitter-typescript 
        tree-sitter-arkts
    )
    target_link_libraries(agent_lib PRIVATE 
        tree-sitter 
        tree-sitter-cpp 
        tree-sitter-python 
        tree-sitter-typescript 
        tree-sitter-arkts
    )
    target_include_directories(photon PRIVATE
        third_party/tree-sitter-typescript/bindings/c
        third_party/tree-sitter-arkts/bindings/c
    )
    target_include_directories(agent_lib PRIVATE
        third_party/tree-sitter-typescript/bindings/c
        third_party/tree-sitter-arkts/bindings/c
    )
    target_compile_definitions(photon PRIVATE PHOTON_ENABLE_TREESITTER)
    target_compile_definitions(agent_lib PRIVATE PHOTON_ENABLE_TREESITTER)
endif()
