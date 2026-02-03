#include "ProjectMemory.h"
#include <fstream>
#include <cstdlib>

ProjectMemory::ProjectMemory(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {
    
    fs::path memoryDir = this->rootPath / ".photon" / "memory";
    fs::create_directories(memoryDir);
    
    memoryFile = memoryDir / "project.json";
    initializeDefaults();
}

void ProjectMemory::initializeDefaults() {
    data = nlohmann::json::object();
    data["project_type"] = "unknown";
    data["build_system"] = "unknown";
    data["toolchain"] = nlohmann::json::array();
    data["architecture_note"] = "";
    data["coding_conventions"] = nlohmann::json::array();
}

void ProjectMemory::load() {
    if (!fs::exists(memoryFile)) {
        return;
    }
    
    try {
        std::ifstream file(memoryFile);
        if (file.is_open()) {
            file >> data;
            file.close();
        }
    } catch (...) {
        // 加载失败,保持默认值
    }
}

void ProjectMemory::save() {
    try {
        std::ofstream file(memoryFile);
        if (file.is_open()) {
            file << data.dump(2);  // 格式化输出
            file.close();
        }
    } catch (...) {
        // 保存失败,忽略
    }
}

bool ProjectMemory::exists() const {
    return fs::exists(memoryFile);
}

std::string ProjectMemory::detectProjectType() {
    // 检查特征文件
    if (fs::exists(rootPath / "CMakeLists.txt")) {
        return "C++";
    }
    if (fs::exists(rootPath / "package.json")) {
        return "JavaScript/TypeScript";
    }
    if (fs::exists(rootPath / "setup.py") || fs::exists(rootPath / "pyproject.toml")) {
        return "Python";
    }
    if (fs::exists(rootPath / "Cargo.toml")) {
        return "Rust";
    }
    if (fs::exists(rootPath / "go.mod")) {
        return "Go";
    }
    if (fs::exists(rootPath / "pom.xml") || fs::exists(rootPath / "build.gradle")) {
        return "Java";
    }
    
    return "unknown";
}

std::string ProjectMemory::detectBuildSystem() {
    if (fs::exists(rootPath / "CMakeLists.txt")) {
        return "CMake";
    }
    if (fs::exists(rootPath / "Makefile")) {
        return "Make";
    }
    if (fs::exists(rootPath / "package.json")) {
        return "npm/yarn";
    }
    if (fs::exists(rootPath / "Cargo.toml")) {
        return "Cargo";
    }
    if (fs::exists(rootPath / "build.gradle")) {
        return "Gradle";
    }
    if (fs::exists(rootPath / "pom.xml")) {
        return "Maven";
    }
    
    return "unknown";
}

std::vector<std::string> ProjectMemory::detectToolchain() {
    std::vector<std::string> toolchain;
    
    // 检查常见工具是否可用
    auto checkCommand = [](const std::string& cmd) {
        std::string checkCmd = "which " + cmd + " >/dev/null 2>&1";
        return system(checkCmd.c_str()) == 0;
    };
    
    if (checkCommand("gcc")) toolchain.push_back("gcc");
    if (checkCommand("g++")) toolchain.push_back("g++");
    if (checkCommand("clang")) toolchain.push_back("clang");
    if (checkCommand("clang++")) toolchain.push_back("clang++");
    if (checkCommand("cmake")) toolchain.push_back("cmake");
    if (checkCommand("make")) toolchain.push_back("make");
    if (checkCommand("python3")) toolchain.push_back("python3");
    if (checkCommand("python")) toolchain.push_back("python");
    if (checkCommand("node")) toolchain.push_back("node");
    if (checkCommand("npm")) toolchain.push_back("npm");
    if (checkCommand("cargo")) toolchain.push_back("cargo");
    if (checkCommand("rustc")) toolchain.push_back("rustc");
    if (checkCommand("go")) toolchain.push_back("go");
    if (checkCommand("java")) toolchain.push_back("java");
    if (checkCommand("javac")) toolchain.push_back("javac");
    
    return toolchain;
}
