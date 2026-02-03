#pragma once
#include <string>
#include "memory/MemoryManager.h"
#include "memory/ProjectMemory.h"

/**
 * @brief 环境探测器
 * 
 * 在首次启动时自动探测项目环境:
 * - 项目类型
 * - 构建系统
 * - 可用工具链
 * - 语言版本
 * 
 * 探测结果保存到 ProjectMemory,后续启动直接加载
 */
class EnvironmentDetector {
public:
    EnvironmentDetector() = default;

    /**
     * @brief 探测并更新项目环境
     * @param rootPath 项目根目录
     * @param memory 记忆管理器
     * 
     * 这个方法会:
     * 1. 探测项目类型
     * 2. 探测构建系统
     * 3. 探测工具链
     * 4. 探测语言版本
     * 5. 保存到 ProjectMemory
     */
    void detect(const std::string& rootPath, MemoryManager& memory);

private:
    /**
     * @brief 探测语言版本
     * 例如: gcc --version, python --version
     */
    void detectLanguageVersions(const std::string& rootPath, MemoryManager& memory);
    
    /**
     * @brief 执行命令并获取输出
     */
    std::string executeCommand(const std::string& command);
};
