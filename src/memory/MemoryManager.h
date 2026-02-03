#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

// 前向声明
class ProjectMemory;
class FailureMemory;
class UserPreference;

/**
 * @brief 记忆管理器 - 统一的记忆系统入口
 * 
 * 管理三种类型的记忆:
 * 1. ProjectMemory: 项目相关的长期知识
 * 2. FailureMemory: 失败案例和解决方案
 * 3. UserPreference: 用户偏好设置
 * 
 * 核心设计原则:
 * - 记忆 ≠ 上下文
 * - 记忆是结构化的长期资产
 * - 记忆不应该每次都放进 Prompt (节省 token)
 */
class MemoryManager {
public:
    /**
     * @brief 构造函数
     * @param rootPath 项目根目录
     */
    explicit MemoryManager(const std::string& rootPath);
    ~MemoryManager();

    /**
     * @brief 加载所有记忆
     */
    void load();

    /**
     * @brief 保存所有记忆
     */
    void save();

    // ========== ProjectMemory 接口 ==========
    
    /**
     * @brief 获取项目类型
     */
    std::string getProjectType();
    
    /**
     * @brief 获取构建系统
     */
    std::string getBuildSystem();
    
    /**
     * @brief 获取工具链
     */
    std::vector<std::string> getToolchain();
    
    /**
     * @brief 设置架构说明
     */
    void setArchitectureNote(const std::string& note);
    
    /**
     * @brief 添加编码约定
     */
    void addCodingConvention(const std::string& rule);

    // ========== FailureMemory 接口 ==========
    
    /**
     * @brief 记录失败
     */
    void recordFailure(const std::string& tool, 
                      const nlohmann::json& args,
                      const std::string& error);
    
    /**
     * @brief 记录解决方案
     */
    void recordSolution(const std::string& error, 
                       const std::string& solution);
    
    /**
     * @brief 查找类似失败
     */
    bool hasSimilarFailure(const std::string& error);
    
    /**
     * @brief 获取失败的解决方案
     */
    std::string getSolution(const std::string& error);

    // ========== UserPreference 接口 ==========
    
    /**
     * @brief 获取用户偏好
     */
    std::string getPreference(const std::string& key, const std::string& defaultValue = "");
    
    /**
     * @brief 设置用户偏好
     */
    void setPreference(const std::string& key, const std::string& value);

private:
    std::string rootPath;
    std::string memoryPath;  // .photon/memory/
    
    std::unique_ptr<ProjectMemory> projectMemory;
    std::unique_ptr<FailureMemory> failureMemory;
    std::unique_ptr<UserPreference> userPreference;
};
