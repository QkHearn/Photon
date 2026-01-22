#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

struct Config {
    struct LLM {
        std::string apiKey;
        std::string baseUrl;
        std::string model;
        std::string systemRole;
    } llm;

    struct Agent {
        size_t contextThreshold;
        std::vector<std::string> fileExtensions;
        bool useBuiltinTools;
    } agent;

    struct MCPServerConfig {
        std::string name;
        std::string command;
    };
    std::vector<MCPServerConfig> mcpServers;

    static Config load(const std::string& path) {
        std::ifstream f(path);
        nlohmann::json j = nlohmann::json::parse(f);
        
        Config cfg;
        cfg.llm.apiKey = j["llm"]["api_key"];
        cfg.llm.baseUrl = j["llm"]["base_url"];
        cfg.llm.model = j["llm"]["model"];
        cfg.llm.systemRole = j["llm"]["system_role"];
        
        cfg.agent.contextThreshold = j["agent"]["context_threshold"];
        cfg.agent.fileExtensions = j["agent"]["file_extensions"].get<std::vector<std::string>>();
        cfg.agent.useBuiltinTools = j["agent"].value("use_builtin_tools", true);
        
        if (j.contains("mcp_servers")) {
            for (auto& item : j["mcp_servers"]) {
                cfg.mcpServers.push_back({item["name"], item["command"]});
            }
        }
        
        return cfg;
    }
};
