#pragma once
#include "ITool.h"
#include <string>
#include <vector>

// 前向声明
class SemanticManager;
struct SemanticChunk;

/**
 * @brief 语义搜索工具
 * 
 * 基于语义相似度搜索代码库中的相关代码片段
 * 适用于模糊查询、概念搜索、功能查找等场景
 */
class SemanticSearchTool : public ITool {
public:
    explicit SemanticSearchTool(SemanticManager* semanticMgr);
    
    std::string getName() const override { return "semantic_search"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    SemanticManager* semanticMgr;
    
    // 格式化搜索结果
    std::string formatSearchResults(const std::vector<SemanticChunk>& chunks, 
                                    const std::string& query);
};
