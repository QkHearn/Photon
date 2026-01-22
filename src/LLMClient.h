#pragma once
#include <string>
#include <nlohmann/json.hpp>

class LLMClient {
public:
    LLMClient(const std::string& apiKey, 
              const std::string& apiEndpoint = "api.openai.com", 
              const std::string& model = "gpt-4o-mini");
    virtual ~LLMClient() = default;
    
    virtual std::string chat(const std::string& prompt, const std::string& systemRole = "");
    
    virtual std::string summarize(const std::string& text);

private:
    std::string apiKey;
    std::string apiEndpoint;
    std::string modelName;
};
