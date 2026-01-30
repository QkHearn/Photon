#include "core/LLMClient.h"
#ifdef _WIN32
    #include <winsock2.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>
#include <regex>

// ANSI Color Codes
static const std::string RED = "\033[31m";
static const std::string YELLOW = "\033[33m";
static const std::string RESET = "\033[0m";

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
    if (res.is_object() && res.contains("choices") && !res["choices"].empty()) {
        auto& msg = res["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null()) {
            return msg["content"];
        }
    }
    return "";
}

// ---------------------------------------------------------------------------
// LLM Request Adapter: normalizeForKimi
// ---------------------------------------------------------------------------
// Design (not workaround): Kimi Chat API has response/request schema inconsistency:
// - Response: assistant message "content" may be array, e.g. [{"type":"text","text":"..."}]
// - Request:  "content" must be string; array is rejected (invalid_request_error).
// Photon normalizes messages before every send so the request is valid.
// See docs/llm-adapters.md for full rationale and behavior.
// ---------------------------------------------------------------------------
static nlohmann::json normalizeForKimi(const nlohmann::json& messages) {
    if (!messages.is_array()) return messages;
    nlohmann::json out = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (!msg.is_object()) continue;
        nlohmann::json m = msg;
        // assistant (and any role): content must be string for request
        if (m.contains("content")) {
            if (m["content"].is_null())
                m["content"] = "";
            else if (m["content"].is_array()) {
                std::string flat;
                for (const auto& part : m["content"]) {
                    if (part.is_object() && part.contains("text") && part["text"].is_string())
                        flat += part["text"].get<std::string>();
                }
                m["content"] = flat.empty() ? "" : flat;
            }
        }
        // tool: Kimi may reject "name" in request; keep only role, tool_call_id, content
        if (m.contains("role") && m["role"] == "tool" && m.contains("name"))
            m.erase("name");
        out.push_back(m);
    }
    return out;
}

nlohmann::json LLMClient::chatWithTools(const nlohmann::json& messages, const nlohmann::json& tools) {
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    nlohmann::json body = {
        {"model", modelName},
        {"messages", normalizeForKimi(messages)}
    };

    if (!tools.empty()) {
        body["tools"] = tools;
    }

    std::string endpoint = pathPrefix + "/chat/completions";
    std::string bodyStr = body.dump();

    httplib::Result res;
    int retryCount = 0;
    const int maxRetries = 3;
    
    while (retryCount < maxRetries) {
        try {
            if (isSsl) {
                httplib::SSLClient cli(host, port);
                cli.set_follow_location(true);
                cli.set_connection_timeout(10);
                cli.set_read_timeout(60);
                res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
            } else {
                httplib::Client cli(host, port);
                cli.set_follow_location(true);
                cli.set_connection_timeout(10);
                cli.set_read_timeout(60);
                res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
            }
            
            if (res && res->status == 200) break;
            
            // If we get here, it's either a connection failure or non-200 status
            retryCount++;
            if (retryCount < maxRetries) {
                std::cerr << YELLOW << "  ⚠ API request failed (Status: " << (res ? std::to_string(res->status) : "Timeout") 
                          << "). Retrying (" << retryCount << "/" << maxRetries << ")..." << RESET << "\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::seconds(2 * retryCount));
            }
        } catch (...) {
            retryCount++;
            if (retryCount >= maxRetries) throw;
            std::this_thread::sleep_for(std::chrono::seconds(2 * retryCount));
        }
    }

    if (res && res->status == 200) {
        return nlohmann::json::parse(res->body);
    } else {
        std::cerr << RED << "✖ API Error after " << maxRetries << " attempts: " 
                  << (res ? std::to_string(res->status) : "Connection failed") << RESET << std::endl;
        if (res && !res->body.empty()) std::cerr << "  Body: " << res->body << std::endl;
        return nlohmann::json::object();
    }
}

std::string LLMClient::summarize(const std::string& text) {
    std::string prompt = "Please summarize the following content briefly while preserving key information:\n\n" + text;
    return chat(prompt, "You are an expert summarizer. Your goal is to compress information while maintaining context.");
}

std::vector<float> LLMClient::getEmbedding(const std::string& text) {
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    nlohmann::json body = {
        {"model", "text-embedding-3-small"}, // Default embedding model
        {"input", text}
    };

    std::string endpoint = pathPrefix + "/embeddings";
    std::string bodyStr = body.dump();

    httplib::Result res;
    try {
        if (isSsl) {
            httplib::SSLClient cli(host, port);
            cli.set_follow_location(true);
            cli.set_connection_timeout(10);
            cli.set_read_timeout(60);
            res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
        } else {
            httplib::Client cli(host, port);
            cli.set_follow_location(true);
            cli.set_connection_timeout(10);
            cli.set_read_timeout(60);
            res = cli.Post(endpoint.c_str(), headers, bodyStr, "application/json");
        }
    } catch (...) {
        return {};
    }

    if (res && res->status == 200) {
        try {
            auto j = nlohmann::json::parse(res->body);
            if (j.contains("data") && !j["data"].empty()) {
                return j["data"][0]["embedding"].get<std::vector<float>>();
            }
        } catch (...) {}
    }
    return {};
}
