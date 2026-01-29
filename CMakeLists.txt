cmake_minimum_required(VERSION 3.15)
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
    include_directories("/opt/homebrew/include")
    link_directories("/opt/homebrew/lib")
endif()

# --- FTXUI Integration ---
add_subdirectory(third_party/ftxui)

find_package(nlohmann_json REQUIRED)
# Optional SQLite3 for semantic index storage
find_package(SQLite3)
# cpp-httplib is often header-only, if not found via find_package, 
# we expect it to be in the include path.

add_executable(photon src/core/main.cpp)

# Add src to include path
target_include_directories(photon PRIVATE src third_party/httplib)

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
    src/utils/FileManager.cpp 
    src/utils/SymbolManager.cpp
    src/utils/LogicMapper.cpp
    src/utils/Logger.cpp
    src/utils/SemanticManager.cpp
    src/utils/RegexSymbolProvider.cpp
    src/utils/TreeSitterSymbolProvider.cpp
    src/core/UIManager.cpp
    src/core/LLMClient.cpp 
    src/core/ContextManager.cpp
    src/mcp/MCPClient.cpp
    src/mcp/LSPClient.cpp
    src/mcp/InternalMCPClient.cpp
)
target_include_directories(agent_lib PUBLIC src third_party/httplib)
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

# Find GTest
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
find_package(GTest REQUIRED)

add_executable(agent_tests 
    tests/test_FileManager.cpp 
    tests/test_ContextManager.cpp
    tests/test_InternalMCPClient.cpp
)
target_link_libraries(agent_tests PRIVATE GTest::gtest GTest::gtest_main agent_lib)

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
