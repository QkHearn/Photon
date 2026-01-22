#pragma once
#include <string>
#include <vector>
#include <memory>
#include "LLMClient.h"

#include <nlohmann/json.hpp>

class ContextManager {
public:
    ContextManager(std::shared_ptr<LLMClient> client, size_t thresholdChars = 4000);

    // 检查并压缩消息列表
    nlohmann::json manage(const nlohmann::json& messages);
    
    // 强制压缩当前消息列表
    nlohmann::json forceCompress(const nlohmann::json& messages);

    // 获取当前上下文大小
    size_t getSize(const nlohmann::json& messages) const;

private:
    std::shared_ptr<LLMClient> llmClient;
    size_t threshold;
    
    size_t calculateSize(const nlohmann::json& messages) const;
    std::string messagesToText(const nlohmann::json& messages) const;
};
