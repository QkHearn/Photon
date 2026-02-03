#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

/**
 * @brief 失败记忆 - 存储失败案例和解决方案
 * 
 * 当 Agent 执行工具失败时,记录:
 * - 失败的工具名称
 * - 工具参数
 * - 错误信息
 * - 如何解决的 (如果有)
 * - 时间戳
 * 
 * 目的: 避免重复犯同样的错误
 * 
 * 存储位置: .photon/memory/failures.json
 */
class FailureMemory {
public:
    struct Failure {
        std::string toolName;
        nlohmann::json args;
        std::string error;
        std::string solution;  // 如何解决的
        std::time_t timestamp;
        
        nlohmann::json toJson() const;
        static Failure fromJson(const nlohmann::json& j);
    };

    explicit FailureMemory(const std::string& rootPath);

    /**
     * @brief 加载记忆
     */
    void load();

    /**
     * @brief 保存记忆
     */
    void save();

    /**
     * @brief 记录失败
     */
    void recordFailure(const std::string& tool, 
                      const nlohmann::json& args,
                      const std::string& error);

    /**
     * @brief 记录解决方案
     * 
     * 当用户或 Agent 成功解决一个问题后,
     * 将解决方案记录到对应的失败记录中
     */
    void recordSolution(const std::string& error, 
                       const std::string& solution);

    /**
     * @brief 查找类似失败
     * 
     * 使用简单的字符串匹配判断是否有类似的失败
     * TODO: 可以升级为语义匹配
     */
    bool hasSimilarFailure(const std::string& error);

    /**
     * @brief 获取解决方案
     */
    std::string getSolution(const std::string& error);

    /**
     * @brief 获取所有失败记录 (用于调试)
     */
    const std::vector<Failure>& getFailures() const { return failures; }

private:
    fs::path rootPath;
    fs::path memoryFile;  // .photon/memory/failures.json
    std::vector<Failure> failures;
    
    const size_t maxFailures = 100;  // 最多保留 100 条失败记录
    
    /**
     * @brief 计算错误相似度
     * 简单的字符串相似度判断
     */
    double calculateSimilarity(const std::string& error1, const std::string& error2);
};
