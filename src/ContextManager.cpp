#include "ContextManager.h"
#include <numeric>
#include <iostream>

ContextManager::ContextManager(std::shared_ptr<LLMClient> client, size_t thresholdChars)
    : llmClient(client), threshold(thresholdChars) {}

void ContextManager::addContext(const std::string& info) {
    contextParts.push_back(info);
    manageContext();
}

size_t ContextManager::currentSize() const {
    size_t total = 0;
    for (const auto& s : contextParts) total += s.length();
    return total;
}

std::string ContextManager::getContext() const {
    return std::accumulate(contextParts.begin(), contextParts.end(), std::string(""),
        [](const std::string& a, const std::string& b) {
            return a.empty() ? b : a + "\n---\n" + b;
        });
}

void ContextManager::manageContext() {
    if (currentSize() > threshold) {
        std::cout << "[ContextManager] Threshold exceeded (" << currentSize() << " > " << threshold 
                  << "). Compressing context..." << std::endl;
        
        std::string fullContext = getContext();
        std::string summary = llmClient->summarize(fullContext);
        
        if (!summary.empty()) {
            contextParts.clear();
            contextParts.push_back("Previous Context Summary: " + summary);
            std::cout << "[ContextManager] Compression successful. New size: " << currentSize() << std::endl;
        } else {
            std::cerr << "[ContextManager] Error: Compression failed, keeping original context." << std::endl;
        }
    }
}

void ContextManager::clear() {
    contextParts.clear();
}
