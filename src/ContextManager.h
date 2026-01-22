#pragma once
#include <string>
#include <vector>
#include <memory>
#include "LLMClient.h"

class ContextManager {
public:
    ContextManager(std::shared_ptr<LLMClient> client, size_t thresholdChars = 4000);

    // 添加新信息到上下文
    void addContext(const std::string& info);

    // 获取当前完整上下文
    std::string getContext() const;

    // 检查是否超过阈值并自动压缩
    void manageContext();

    // 清除上下文
    void clear();

private:
    std::shared_ptr<LLMClient> llmClient;
    std::vector<std::string> contextParts;
    size_t threshold;
    
    size_t currentSize() const;
};
