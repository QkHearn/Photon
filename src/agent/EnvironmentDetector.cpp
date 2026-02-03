#include "EnvironmentDetector.h"
#include <iostream>
#include <memory>
#include <array>

std::string EnvironmentDetector::executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

void EnvironmentDetector::detect(const std::string& rootPath, MemoryManager& memory) {
    std::cout << "[EnvironmentDetector] Detecting project environment..." << std::endl;
    
    // 1. 探测项目类型
    std::cout << "[EnvironmentDetector] Detecting project type..." << std::endl;
    // TODO: 实际调用 ProjectMemory 的探测方法
    // 由于 MemoryManager 没有直接暴露 ProjectMemory,需要添加接口
    
    // 2. 探测构建系统
    std::cout << "[EnvironmentDetector] Detecting build system..." << std::endl;
    
    // 3. 探测工具链
    std::cout << "[EnvironmentDetector] Detecting toolchain..." << std::endl;
    
    // 4. 探测语言版本
    detectLanguageVersions(rootPath, memory);
    
    // 5. 保存记忆
    memory.save();
    
    std::cout << "[EnvironmentDetector] Environment detection complete." << std::endl;
}

void EnvironmentDetector::detectLanguageVersions(const std::string& rootPath, MemoryManager& memory) {
    // 探测常见语言版本
    std::cout << "[EnvironmentDetector] Detecting language versions..." << std::endl;
    
    // C++ 编译器
    std::string gccVersion = executeCommand("gcc --version 2>/dev/null | head -1");
    if (!gccVersion.empty()) {
        std::cout << "[EnvironmentDetector]   gcc: " << gccVersion;
    }
    
    std::string clangVersion = executeCommand("clang --version 2>/dev/null | head -1");
    if (!clangVersion.empty()) {
        std::cout << "[EnvironmentDetector]   clang: " << clangVersion;
    }
    
    // Python
    std::string pythonVersion = executeCommand("python3 --version 2>/dev/null");
    if (!pythonVersion.empty()) {
        std::cout << "[EnvironmentDetector]   python3: " << pythonVersion;
    }
    
    // Node.js
    std::string nodeVersion = executeCommand("node --version 2>/dev/null");
    if (!nodeVersion.empty()) {
        std::cout << "[EnvironmentDetector]   node: " << nodeVersion;
    }
    
    // TODO: 将版本信息保存到记忆中
}
