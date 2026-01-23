cmake_minimum_required(VERSION 3.10)
project(Photon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows specific global settings
if(WIN32)
    # Static link for MSVC runtime (/MT) - MUST be set before any targets
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
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

find_package(nlohmann_json REQUIRED)
# cpp-httplib is often header-only, if not found via find_package, 
# we expect it to be in the include path.

add_executable(photon 
    src/main.cpp 
    src/FileManager.cpp 
    src/LLMClient.cpp 
    src/ContextManager.cpp
    src/MCPClient.cpp
    src/InternalMCPClient.cpp
)

target_link_libraries(photon PRIVATE nlohmann_json::nlohmann_json)

# --- Unit Tests ---
enable_testing()

# Create a library for shared logic to be tested
add_library(agent_lib 
    src/FileManager.cpp 
    src/LLMClient.cpp 
    src/ContextManager.cpp
    src/MCPClient.cpp
    src/InternalMCPClient.cpp
)
target_link_libraries(agent_lib PRIVATE nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto)

# Link main app to the library
target_link_libraries(photon PRIVATE agent_lib)

# Find GTest
find_package(GTest REQUIRED)

add_executable(agent_tests 
    tests/test_FileManager.cpp 
    tests/test_ContextManager.cpp
)
target_link_libraries(agent_tests PRIVATE GTest::GTest GTest::Main agent_lib)

add_test(NAME AgentTests COMMAND agent_tests)

# On Linux/macOS, we might need OpenSSL for HTTPS
find_package(OpenSSL REQUIRED)
target_link_libraries(photon PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# On Windows, we need to link against ws2_32 and crypt32
if(WIN32)
    target_link_libraries(photon PRIVATE ws2_32 crypt32 bcrypt)
    target_link_libraries(agent_lib PRIVATE ws2_32 crypt32 bcrypt)
    target_link_libraries(agent_tests PRIVATE ws2_32 crypt32 bcrypt)
endif()

# Link pthread if needed
find_package(Threads REQUIRED)
target_link_libraries(photon PRIVATE Threads::Threads)
