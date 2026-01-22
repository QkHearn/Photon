#pragma once
#include <vector>
#include <memory>
#include <map>
#include "MCPClient.h"
#include "InternalMCPClient.h"
#include "ConfigManager.h"

class MCPManager {
public:
    // 初始化内置工具
    void initBuiltin(const std::string& rootPath) {
        auto client = std::make_unique<InternalMCPClient>(rootPath);
        clients["builtin"] = std::move(client);
        std::cout << "[MCPManager] Built-in tools initialized." << std::endl;
    }

    // 根据配置初始化所有服务器
    void initFromConfig(const std::vector<Config::MCPServerConfig>& configs) {
        for (const auto& cfg : configs) {
            auto client = std::make_unique<MCPClient>(cfg.command);
            if (client->initialize()) {
                std::cout << "[MCPManager] Connected to: " << cfg.name << std::endl;
                clients[cfg.name] = std::move(client);
            }
        }
    }

    // 获取所有服务器的工具定义，供 LLM 参考
    nlohmann::json getAllTools() {
        nlohmann::json allTools = nlohmann::json::array();
        for (auto& [name, client] : clients) {
            auto tools = client->listTools();
            if (tools.contains("result") && tools["result"].contains("tools")) {
                for (auto& t : tools["result"]["tools"]) {
                    t["server_name"] = name; // 标记所属服务器
                    allTools.push_back(t);
                }
            }
        }
        return allTools;
    }

    // 调用特定服务器的工具
    nlohmann::json callTool(const std::string& serverName, const std::string& toolName, const nlohmann::json& args) {
        if (clients.count(serverName)) {
            return clients[serverName]->callTool(toolName, args);
        }
        return {{"error", "Server not found"}};
    }

    std::string getLastModifiedFile(const std::string& serverName) {
        if (clients.count(serverName)) {
            return clients[serverName]->getLastFile();
        }
        return "";
    }

private:
    std::map<std::string, std::unique_ptr<IMCPClient>> clients;
};
