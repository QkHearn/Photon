#include "LLMClient.h"
#ifdef _WIN32
    #include <winsock2.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>
#include <regex>

LLMClient::LLMClient(const std::string& apiKey, const std::string& baseUrl, const std::string& model) 
    : apiKey(apiKey), baseUrl(baseUrl), modelName(model) {
    parseBaseUrl(baseUrl);
}

void LLMClient::parseBaseUrl(const std::string& url) {
    std::regex urlRegex(R"((http|https)://([^/:]+)(?::(\d+))?(.*))");
    std::smatch match;
    if (std::regex_match(url, match, urlRegex)) {
        isSsl = (match[1] == "https");
        host = match[2];
        if (match[3].matched) {
            port = std::stoi(match[3]);
        } else {
            port = isSsl ? 443 : 80;
        }
        pathPrefix = match[4];
        if (pathPrefix.empty()) {
            pathPrefix = "";
        }
    } else {
        // Default to legacy behavior if regex fails
        isSsl = true;
        host = url;
        port = 443;
        pathPrefix = "";
    }
}

std::string LLMClient::chat(const std::string& prompt, const std::string& systemRole) {
    nlohmann::json messages = nlohmann::json::array();
    std::string role = systemRole.empty() ? "You are a helpful assistant." : systemRole;
    messages.push_back({{"role", "system"}, {"content", role}});
    messages.push_back({{"role", "user"}, {"content", prompt}});

    nlohmann::json res = chatWithTools(messages, nlohmann::json::array());
    if (res.contains("choices") && !res["choices"].empty()) {
        auto& msg = res["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null()) {
            return msg["content"];
        }
    }
    return "";
}

nlohmann::json LLMClient::chatWithTools(const nlohmann::json& messages, const nlohmann::json& tools) {
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    nlohmann::json body = {
        {"model", modelName},
        {"messages", messages}
    };

    if (!tools.empty()) {
        body["tools"] = tools;
    }

    std::string endpoint = pathPrefix + "/chat/completions";
    std::string bodyStr = body.dump();

    httplib::Result res;
    if (isSsl) {
        httplib::SSLClient cli(host, port);
        cli.set_follow_location(true);
        res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
    } else {
        httplib::Client cli(host, port);
        cli.set_follow_location(true);
        res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
    }

    if (res && res->status == 200) {
        return nlohmann::json::parse(res->body);
    } else {
        std::cerr << "API Error: " << (res ? std::to_string(res->status) : "Connection failed") << " for host " << host << std::endl;
        if (res) std::cerr << "Body: " << res->body << std::endl;
        return nlohmann::json::object();
    }
}

std::string LLMClient::summarize(const std::string& text) {
    std::string prompt = "Please summarize the following content briefly while preserving key information:\n\n" + text;
    return chat(prompt, "You are an expert summarizer. Your goal is to compress information while maintaining context.");
}
