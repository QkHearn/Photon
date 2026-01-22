#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include "FileManager.h"
#include "LLMClient.h"
#include "ContextManager.h"
#include "ConfigManager.h"
#include "MCPClient.h"
#include "MCPManager.h"

void printUsage() {
    std::cout << "Usage: photon <directory_path> [config_path]" << std::endl;
}

nlohmann::json formatToolsForLLM(const nlohmann::json& mcpTools) {
    nlohmann::json llmTools = nlohmann::json::array();
    for (const auto& t : mcpTools) {
        nlohmann::json tool;
        tool["type"] = "function";
        nlohmann::json function;
        // Combine server name and tool name to ensure uniqueness and allow dispatching
        function["name"] = t["server_name"].get<std::string>() + "__" + t["name"].get<std::string>();
        function["description"] = t["description"];
        function["parameters"] = t["inputSchema"];
        tool["function"] = function;
        llmTools.push_back(tool);
    }
    return llmTools;
}

int main(int argc, char* argv[]) {
    std::string path = ".";
    std::string configPath = "../config.json";

    if (argc >= 2) {
        path = argv[1];
    }
    if (argc >= 3) {
        configPath = argv[2];
    }

    Config cfg;
    try {
        cfg = Config::load(configPath);
        std::cout << "Loaded configuration from: " << configPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return 1;
    }

    auto llmClient = std::make_shared<LLMClient>(cfg.llm.apiKey, cfg.llm.baseUrl, cfg.llm.model);
    ContextManager contextManager(llmClient, cfg.agent.contextThreshold);

    // Initialize MCP Manager and connect all servers
    MCPManager mcpManager;
    if (cfg.agent.useBuiltinTools) {
        mcpManager.initBuiltin(path);
    }
    mcpManager.initFromConfig(cfg.mcpServers);
    
    // Get all available tools and format them for the LLM
    auto mcpTools = mcpManager.getAllTools();
    nlohmann::json llmTools = formatToolsForLLM(mcpTools);

    std::cout << "--- Photon Agent ---" << std::endl;
    std::cout << "Target directory: " << path << std::endl;

    // Optimization: Don't load all files. Just tell the LLM the root path.
    std::string systemPrompt = cfg.llm.systemRole + "\n" +
                               "Current working directory for your tools: " + path + "\n" +
                               "You can use the provided MCP tools to explore and read files in this directory. " +
                               "Always use relative paths from the current working directory.";
    
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    std::string userInput;
    while (true) {
        std::cout << "\nYou: ";
        if (!std::getline(std::cin, userInput) || userInput == "exit") break;
        if (userInput.empty()) continue;

        messages.push_back({{"role", "user"}, {"content", userInput}});

        bool continues = true;
        while (continues) {
            nlohmann::json response = llmClient->chatWithTools(messages, llmTools);
            
            if (response.is_null() || !response.contains("choices") || response["choices"].empty()) {
                std::cerr << "Error: No response from LLM" << std::endl;
                break;
            }

            auto& choice = response["choices"][0];
            auto& message = choice["message"];
            
            // Add assistant's message to the conversation history
            messages.push_back(message);

            if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                for (auto& toolCall : message["tool_calls"]) {
                    std::string fullName = toolCall["function"]["name"];
                    std::string argsStr = toolCall["function"]["arguments"];
                    nlohmann::json args = nlohmann::json::parse(argsStr);

                    // Split server_name__tool_name
                    size_t pos = fullName.find("__");
                    if (pos != std::string::npos) {
                        std::string serverName = fullName.substr(0, pos);
                        std::string toolName = fullName.substr(pos + 2);

                        std::cout << "[MCP Call] Server: " << serverName << ", Tool: " << toolName << ", Args: " << args.dump() << std::endl;
                        
                        nlohmann::json result = mcpManager.callTool(serverName, toolName, args);
                        
                        // Add tool result to messages
                        messages.push_back({
                            {"role", "tool"},
                            {"tool_call_id", toolCall["id"]},
                            {"name", fullName},
                            {"content", result.dump()}
                        });
                    }
                }
                // Continue the loop to let LLM process tool results
                continues = true;
            } else {
                // No more tool calls, this is the final answer
                std::cout << "\nAgent: " << message["content"].get<std::string>() << std::endl;
                continues = false;
            }
        }
    }

    return 0;
}
