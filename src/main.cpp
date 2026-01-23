#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <iomanip>
#include <regex>
#include "FileManager.h"
#include "LLMClient.h"
#include "ContextManager.h"
#include "ConfigManager.h"
#include "MCPClient.h"
#include "MCPManager.h"

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string CYAN = "\033[36m";
const std::string MAGENTA = "\033[35m";
const std::string YELLOW = "\033[33m";
const std::string GREEN = "\033[32m";
const std::string BLUE = "\033[34m";
const std::string RED = "\033[31m";
const std::string ITALIC = "\033[3m";

std::string renderMarkdown(const std::string& input) {
    std::string output = input;
    
    // 1. Code Blocks (MUST be processed first to avoid inner styling)
    // More robust regex: allows optional language, handles missing newlines gracefully
    std::regex code_block(R"(```([a-z]*)\s*([\s\S]*?)\s*```)");
    output = std::regex_replace(output, code_block, YELLOW + "╭────────── $1 ──────────\n$2\n╰────────────────────────" + RESET);

    // 2. Horizontal Rules
    output = std::regex_replace(output, std::regex(R"(^---$)", std::regex_constants::multiline), CYAN + "──────────────────────────────────────────────────" + RESET);

    // 3. Headers
    output = std::regex_replace(output, std::regex(R"(^# (.*))", std::regex_constants::multiline), BOLD + MAGENTA + "█ $1" + RESET);
    output = std::regex_replace(output, std::regex(R"(^## (.*))", std::regex_constants::multiline), BOLD + CYAN + "▓ $1" + RESET);
    output = std::regex_replace(output, std::regex(R"(^### (.*))", std::regex_constants::multiline), BOLD + BLUE + "▒ $1" + RESET);
    output = std::regex_replace(output, std::regex(R"(^#### (.*))", std::regex_constants::multiline), BOLD + YELLOW + "░ $1" + RESET);
    
    // 4. Blockquotes
    output = std::regex_replace(output, std::regex(R"(^> (.*))", std::regex_constants::multiline), BLUE + "┃ " + RESET + ITALIC + "$1" + RESET);
    
    // 5. Bold & Italic
    output = std::regex_replace(output, std::regex(R"(\*\*\*(.*?)\*\*\*)"), BOLD + ITALIC + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*\*(.*?)\*\*)"), BOLD + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*(.*?)\*)"), ITALIC + "$1" + RESET);
    
    // 6. Inline Code (Avoid matching triple backticks)
    output = std::regex_replace(output, std::regex(R"(`([^`]+)`)"), GREEN + " $1 " + RESET);
    
    // 7. Links
    output = std::regex_replace(output, std::regex(R"(\[(.*?)\]\((.*?)\))"), BLUE + "$1" + RESET + " (" + CYAN + "$2" + RESET + ")");
    
    // 8. Lists
    output = std::regex_replace(output, std::regex(R"(^[\s]*- )", std::regex_constants::multiline), "  " + CYAN + "•" + RESET + " ");
    output = std::regex_replace(output, std::regex(R"(^[\s]*\d+\. )", std::regex_constants::multiline), "  " + CYAN + "$&" + RESET);
    
    // 9. Tables (Simple terminal styling)
    // Format separator row |---|---|
    output = std::regex_replace(output, std::regex(R"(^\|[-:| ]+\|.*$)", std::regex_constants::multiline), CYAN + "$&" + RESET);
    // Style table pipes
    output = std::regex_replace(output, std::regex(R"(^\|)", std::regex_constants::multiline), CYAN + "┃" + RESET);
    output = std::regex_replace(output, std::regex(R"(\|)", std::regex_constants::multiline), CYAN + "┃" + RESET);

    return output;
}

void printLogo() {
    std::cout << GREEN << BOLD << R"(
      _____  _           _              
     |  __ \| |         | |             
     | |__) | |__   ___ | |_ ___  _ __  
     |  ___/| '_ \ / _ \| __/ _ \| '_ \ 
     | |    | | | | (_) | || (_) | | | |
     |_|    |_| |_|\___/ \__\___/|_| |_|
    )" << RESET << std::endl;
    std::cout << CYAN << "      --- Quantum-Powered AI Code Agent ---" << RESET << "\n" << std::endl;
}

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
    std::string configPath = "config.json"; // Default to current directory

    if (argc >= 2) {
        path = argv[1];
    }
    
    // Check if config.json exists in current directory, if not try parent (for build/ dir runs)
    if (argc < 3) {
        if (!fs::exists(configPath) && fs::exists("../config.json")) {
            configPath = "../config.json";
        }
    } else {
        configPath = argv[2];
    }

    Config cfg;
    try {
        if (!fs::exists(configPath)) {
            throw std::runtime_error("Configuration file not found: " + configPath);
        }
        cfg = Config::load(configPath);
        std::cout << GREEN << "✔ Loaded configuration from: " << BOLD << configPath << RESET << std::endl;
    } catch (const std::exception& e) {
        std::cerr << RED << "✖ Failed to load config: " << e.what() << RESET << std::endl;
        printUsage();
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

    printLogo();
    std::cout << BLUE << "Target directory: " << BOLD << path << RESET << std::endl;
    std::cout << BLUE << "Configuration:    " << BOLD << configPath << RESET << std::endl;
    std::cout << YELLOW << "Shortcuts: " << RESET 
              << BOLD << "clear" << RESET << " (forget), " 
              << BOLD << "compress" << RESET << " (active summary), " 
              << BOLD << "undo" << RESET << " (revert last file change)" << std::endl;

    // Optimization: Don't load all files. Just tell the LLM the root path.
    std::string systemPrompt = cfg.llm.systemRole + "\n" +
                               "Current working directory for your tools: " + path + "\n" +
                               "You can use the provided MCP tools to explore and read files in this directory. " +
                               "Always use relative paths from the current working directory.";
    
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    std::string userInput;
    while (true) {
        std::cout << "\n" << CYAN << BOLD << "You > " << RESET;
        if (!std::getline(std::cin, userInput) || userInput == "exit") break;
        if (userInput.empty()) continue;
        
        if (userInput == "clear") {
            messages = nlohmann::json::array();
            messages.push_back({{"role", "system"}, {"content", systemPrompt}});
            std::cout << GREEN << "✔ Context cleared (Forgotten)." << RESET << std::endl;
            continue;
        }

        if (userInput == "compress") {
            messages = contextManager.forceCompress(messages);
            continue;
        }

        if (userInput == "undo") {
            std::string lastFile = mcpManager.getLastModifiedFile("builtin");
            if (lastFile.empty()) {
                std::cout << YELLOW << "⚠ No recent file modifications recorded." << RESET << std::endl;
            } else {
                std::cout << YELLOW << "Attempting to undo last modification to: " << BOLD << lastFile << RESET << std::endl;
                nlohmann::json result = mcpManager.callTool("builtin", "file_undo", {{"path", lastFile}});
                if (result.contains("content")) {
                    std::cout << GREEN << "✔ " << result["content"][0]["text"].get<std::string>() << RESET << std::endl;
                } else {
                    std::cout << RED << "✖ Undo failed: " << result.dump() << RESET << std::endl;
                }
            }
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", userInput}});

        bool continues = true;
        while (continues) {
            // Apply context management
            messages = contextManager.manage(messages);

            std::cout << YELLOW << "Thinking..." << RESET << "\r" << std::flush;
            nlohmann::json response = llmClient->chatWithTools(messages, llmTools);
            
            if (response.is_null() || !response.contains("choices") || response["choices"].empty()) {
                std::cerr << RED << "Error: No response from LLM" << RESET << std::endl;
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

                        std::cout << YELLOW << "⚙️  [MCP Call] " << RESET 
                                  << CYAN << serverName << RESET << "::" << BOLD << toolName << RESET 
                                  << " " << args.dump() << std::endl;
                        
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
                std::cout << "           " << "\r"; // Clear "Thinking..."
                std::string content = message["content"].get<std::string>();
                std::cout << MAGENTA << BOLD << "Photon > " << RESET << renderMarkdown(content) << std::endl;
                
                // Beautiful Context Usage Display
                size_t currentSize = contextManager.getSize(messages);
                std::cout << CYAN << "── Memory: " << BOLD << currentSize << RESET << CYAN << " chars ──" << RESET << std::endl;
                
                continues = false;
            }
        }
    }

    return 0;
}
