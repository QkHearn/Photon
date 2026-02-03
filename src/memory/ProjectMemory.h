#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief 项目记忆 - 存储项目相关的长期知识
 * 
 * 内容包括:
 * - 项目类型 (C++, Python, TypeScript, etc.)
 * - 构建系统 (CMake, Make, npm, etc.)
 * - 工具链 (gcc, clang, python, node, etc.)
 * - 架构说明
 * - 编码约定
 * 
 * 这些信息应该在项目首次扫描时自动探测,
 * 并保存到 .photon/memory/project.json
 */
class ProjectMemory {
public:
    explicit ProjectMemory(const std::string& rootPath);

    /**
     * @brief 加载记忆
     */
    void load();

    /**
     * @brief 保存记忆
     */
    void save();

    /**
     * @brief 检查记忆是否存在
     */
    bool exists() const;

    // ========== 自动探测 ==========
    
    /**
     * @brief 探测项目类型
     * 通过文件特征判断 (CMakeLists.txt, package.json, setup.py, etc.)
     */
    std::string detectProjectType();
    
    /**
     * @brief 探测构建系统
     * 通过检查构建文件和命令可用性判断
     */
    std::string detectBuildSystem();
    
    /**
     * @brief 探测工具链
     * 检查常见编译器和工具的可用性
     */
    std::vector<std::string> detectToolchain();

    // ========== 访问接口 ==========
    
    std::string getProjectType() const { return data["project_type"].get<std::string>(); }
    std::string getBuildSystem() const { return data["build_system"].get<std::string>(); }
    std::vector<std::string> getToolchain() const { 
        return data["toolchain"].get<std::vector<std::string>>(); 
    }
    
    void setArchitectureNote(const std::string& note) { 
        data["architecture_note"] = note; 
    }
    
    void addCodingConvention(const std::string& rule) {
        if (!data.contains("coding_conventions")) {
            data["coding_conventions"] = nlohmann::json::array();
        }
        data["coding_conventions"].push_back(rule);
    }

private:
    fs::path rootPath;
    fs::path memoryFile;  // .photon/memory/project.json
    nlohmann::json data;
    
    void initializeDefaults();
};
