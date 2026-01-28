#pragma once
#include <string>
#include <nlohmann/json.hpp>

class LLMClient {
public:
    LLMClient(const std::string& apiKey, 
              const std::string& baseUrl = "https://api.openai.com/v1", 
              const std::string& model = "gpt-4o-mini");
    virtual ~LLMClient() = default;
    
    virtual std::string chat(const std::string& prompt, const std::string& systemRole = "");
    virtual nlohmann::json chatWithTools(const nlohmann::json& messages, const nlohmann::json& tools);
    
    virtual std::string summarize(const std::string& text);
    virtual std::vector<float> getEmbedding(const std::string& text);

private:
    std::string apiKey;
    std::string baseUrl;
    std::string modelName;
    bool isSsl;
    std::string host;
    int port;
    std::string pathPrefix;

    void parseBaseUrl(const std::string& url);
};
