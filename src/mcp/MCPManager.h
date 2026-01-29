#pragma once
#include <vector>
#include <memory>
#include <map>
#include "mcp/MCPClient.h"
#include "mcp/InternalMCPClient.h"
#include "core/ConfigManager.h"

#include <future>

class MCPManager {
public:
    // 初始化内置工具
    void initBuiltin(const std::string& rootPath, const std::string& searchApiKey = "") {
        auto client = std::make_unique<InternalMCPClient>(rootPath);
        if (!searchApiKey.empty()) {
            client->setSearchApiKey(searchApiKey);
        }
        clients["builtin"] = std::move(client);
    }

    // 根据配置初始化所有服务器 (并行异步)
    int initFromConfig(const std::vector<Config::MCPServerConfig>& configs) {
        if (configs.empty()) return 0;

        std::vector<std::pair<std::string, std::future<std::unique_ptr<MCPClient>>>> futures;
        for (const auto& cfg : configs) {
            futures.push_back({cfg.name, std::async(std::launch::async, [cmd = cfg.command]() {
                auto client = std::make_unique<MCPClient>(cmd);
                if (client->initialize()) {
                    return client;
                }
                return std::unique_ptr<MCPClient>(nullptr);
            })});
        }

        int count = 0;
        for (auto& f : futures) {
            auto client = f.second.get();
            if (client) {
                clients[f.first] = std::move(client);
                count++;
            }
        }
        return count;
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

    int getTotalTaskCount() {
        int total = 0;
        for (auto& [name, client] : clients) {
            total += client->getTaskCount();
        }
        return total;
    }

    void setAllAuthorized(bool authorized) {
        for (auto& [name, client] : clients) {
            client->setAuthorized(authorized);
        }
    }

    IMCPClient* getClient(const std::string& name) {
        if (clients.count(name)) {
            return clients[name].get();
        }
        return nullptr;
    }

private:
    std::map<std::string, std::unique_ptr<IMCPClient>> clients;
};
