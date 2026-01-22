#include "ContextManager.h"
#include <numeric>
#include <iostream>

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string YELLOW = "\033[33m";
const std::string GREEN = "\033[32m";

ContextManager::ContextManager(std::shared_ptr<LLMClient> client, size_t thresholdChars)
    : llmClient(client), threshold(thresholdChars) {}

size_t ContextManager::calculateSize(const nlohmann::json& messages) const {
    size_t total = 0;
    for (const auto& msg : messages) {
        if (msg.contains("content") && msg["content"].is_string()) {
            total += msg["content"].get<std::string>().length();
        }
        if (msg.contains("tool_calls")) {
            total += msg["tool_calls"].dump().length();
        }
    }
    return total;
}

size_t ContextManager::getSize(const nlohmann::json& messages) const {
    return calculateSize(messages);
}

std::string ContextManager::messagesToText(const nlohmann::json& messages) const {
    std::string text;
    for (const auto& msg : messages) {
        std::string role = msg["role"];
        if (msg.contains("content") && msg["content"].is_string()) {
            text += role + ": " + msg["content"].get<std::string>() + "\n";
        }
    }
    return text;
}

nlohmann::json ContextManager::forceCompress(const nlohmann::json& messages) {
    if (messages.size() <= 2) return messages;
    
    std::cout << YELLOW << "[ContextManager] Manual compression triggered..." << RESET << std::endl;
    
    nlohmann::json managedMessages = nlohmann::json::array();
    nlohmann::json toSummarize = nlohmann::json::array();
    
    managedMessages.push_back(messages[0]); // System Prompt
    
    // Keep only the very last message for context after manual compression
    for (size_t i = 1; i < messages.size() - 1; ++i) {
        toSummarize.push_back(messages[i]);
    }
    
    std::string summary = llmClient->summarize(messagesToText(toSummarize));
    if (!summary.empty()) {
        managedMessages.push_back({
            {"role", "system"},
            {"content", "Summary of earlier conversation: " + summary}
        });
    }
    
    managedMessages.push_back(messages.back());
    
    std::cout << GREEN << "✔ Context manually compressed." << RESET << std::endl;
    return managedMessages;
}

nlohmann::json ContextManager::manage(const nlohmann::json& messages) {
    size_t currentSize = calculateSize(messages);
    if (currentSize <= threshold || messages.size() <= 6) {
        return messages;
    }

    std::cout << YELLOW << "[ContextManager] Threshold reached (" << currentSize << " > " << threshold 
              << "). Compressing intermediate history..." << RESET << std::endl;

    nlohmann::json managedMessages = nlohmann::json::array();
    nlohmann::json toSummarize = nlohmann::json::array();

    // 策略：
    // 1. 保留第一个 System Prompt
    // 2. 保留最后 4 条消息（保持短期记忆）
    // 3. 压缩中间的消息
    
    managedMessages.push_back(messages[0]); // System Prompt

    size_t lastN = 4;
    size_t middleEnd = messages.size() - lastN;

    for (size_t i = 1; i < middleEnd; ++i) {
        toSummarize.push_back(messages[i]);
    }

    std::string summary = llmClient->summarize(messagesToText(toSummarize));
    
    if (!summary.empty()) {
        managedMessages.push_back({
            {"role", "system"},
            {"content", "Summary of earlier conversation: " + summary}
        });
    }

    for (size_t i = middleEnd; i < messages.size(); ++i) {
        managedMessages.push_back(messages[i]);
    }

    std::cout << GREEN << "✔ Context compressed. New size: " << calculateSize(managedMessages) << RESET << std::endl;
    return managedMessages;
}
