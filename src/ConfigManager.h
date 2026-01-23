#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
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

    static Config load(const std::string& pathStr) {
        std::filesystem::path path = std::filesystem::u8path(pathStr);
        std::ifstream f(path);
        if (!f.is_open()) {
            throw std::runtime_error("Could not open config file: " + pathStr);
        }
        
        // Read the file content into a string first to ensure proper encoding handling
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(content);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("JSON Parse Error in " + path.string() + ": " + e.what());
        }
        
        Config cfg;
        cfg.llm.apiKey = j.at("llm").at("api_key").get<std::string>();
        cfg.llm.baseUrl = j.at("llm").at("base_url").get<std::string>();
        cfg.llm.model = j.at("llm").at("model").get<std::string>();
        cfg.llm.systemRole = j.at("llm").at("system_role").get<std::string>();
        
        cfg.agent.contextThreshold = j.at("agent").at("context_threshold").get<size_t>();
        cfg.agent.fileExtensions = j.at("agent").at("file_extensions").get<std::vector<std::string>>();
        cfg.agent.useBuiltinTools = j.at("agent").value("use_builtin_tools", true);
        
        if (j.contains("mcp_servers")) {
            for (auto& item : j["mcp_servers"]) {
                cfg.mcpServers.push_back({item["name"], item["command"]});
            }
        }
        
        return cfg;
    }
};
