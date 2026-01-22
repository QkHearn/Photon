#include "LLMClient.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>

LLMClient::LLMClient(const std::string& apiKey, const std::string& apiEndpoint, const std::string& model) 
    : apiKey(apiKey), apiEndpoint(apiEndpoint), modelName(model) {}

std::string LLMClient::chat(const std::string& prompt, const std::string& systemRole) {
    httplib::SSLClient cli(apiEndpoint);
    cli.set_follow_location(true);

    std::string role = systemRole.empty() ? "You are a helpful assistant." : systemRole;

    nlohmann::json body = {
        {"model", modelName},
        {"messages", {
            {{"role", "system"}, {"content", role}},
            {{"role", "user"}, {"content", prompt}}
        }}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/chat/completions", headers, body.dump(), "application/json");

    if (res && res->status == 200) {
        auto jsonRes = nlohmann::json::parse(res->body);
        return jsonRes["choices"][0]["message"]["content"];
    } else {
        std::cerr << "API Error: " << (res ? std::to_string(res->status) : "Connection failed") << std::endl;
        if (res) std::cerr << "Body: " << res->body << std::endl;
        return "";
    }
}

std::string LLMClient::summarize(const std::string& text) {
    std::string prompt = "Please summarize the following content briefly while preserving key information:\n\n" + text;
    return chat(prompt, "You are an expert summarizer. Your goal is to compress information while maintaining context.");
}
