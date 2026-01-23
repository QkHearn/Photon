#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <iomanip>
#include <regex>
#include <sstream>
#ifdef _WIN32
    #include <winsock2.h>
#endif
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
    std::string output;
    std::string remaining = input;
    
    // 1. Code Blocks (MUST be processed carefully)
    // We'll replace code blocks with placeholders to avoid line-by-line processing
    std::vector<std::string> codeBlocks;
    std::regex code_block_regex(R"(```([a-z]*)\s*([\s\S]*?)\s*```)");
    auto words_begin = std::sregex_iterator(remaining.begin(), remaining.end(), code_block_regex);
    auto words_end = std::sregex_iterator();

    std::string placeholder_prefix = "___CODE_BLOCK_";
    size_t last_pos = 0;
    std::string text_with_placeholders;
    int block_idx = 0;

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        text_with_placeholders += remaining.substr(last_pos, match.position() - last_pos);
        
        std::string lang = match[1].str();
        std::string code = match[2].str();
        std::string rendered = YELLOW + "╭────────── " + (lang.empty() ? "code" : lang) + " ──────────\n" + code + "\n╰────────────────────────" + RESET;
        
        codeBlocks.push_back(rendered);
        text_with_placeholders += placeholder_prefix + std::to_string(block_idx++) + "___";
        last_pos = match.position() + match.length();
    }
    text_with_placeholders += remaining.substr(last_pos);

    // 2. Process line by line for block elements
    std::stringstream ss(text_with_placeholders);
    std::string line;
    std::string processed_text;
    while (std::getline(ss, line)) {
        // Horizontal Rules
        if (std::regex_match(line, std::regex(R"(^---$)"))) {
            line = CYAN + "──────────────────────────────────────────────────" + RESET;
        }
        // Headers
        else if (std::regex_match(line, std::regex(R"(^# (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^# (.*))"), BOLD + MAGENTA + "█ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^## (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^## (.*))"), BOLD + CYAN + "▓ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^### (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^### (.*))"), BOLD + BLUE + "▒ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^#### (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^#### (.*))"), BOLD + YELLOW + "░ $1" + RESET);
        }
        // Blockquotes
        else if (std::regex_match(line, std::regex(R"(^> (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^> (.*))"), BLUE + "┃ " + RESET + ITALIC + "$1" + RESET);
        }
        // Lists
        else if (std::regex_search(line, std::regex(R"(^[\s]*- )"))) {
            line = std::regex_replace(line, std::regex(R"(^([\s]*)- )"), "$1" + CYAN + "•" + RESET + " ");
        }
        else if (std::regex_search(line, std::regex(R"(^[\s]*\d+\. )"))) {
            line = std::regex_replace(line, std::regex(R"(^([\s]*\d+\. ))"), CYAN + "$1" + RESET);
        }
        // Tables (Simple check)
        else if (std::regex_match(line, std::regex(R"(^\|.*\|$)"))) {
            if (std::regex_match(line, std::regex(R"(^\|[-:| ]+\|.*$)"))) {
                line = CYAN + line + RESET;
            } else {
                // Style table pipes
                line = std::regex_replace(line, std::regex(R"(\| )"), CYAN + "┃ " + RESET);
                line = std::regex_replace(line, std::regex(R"( \|)"), " " + CYAN + "┃" + RESET);
                // Also handle the start/end pipes
                if (!line.empty() && line[0] == '|') line = CYAN + "┃" + RESET + line.substr(1);
                if (!line.empty() && line.back() == '|') line = line.substr(0, line.size()-1) + CYAN + "┃" + RESET;
            }
        }

        processed_text += line + "\n";
    }

    // 3. Inline Elements (Bold, Italic, Code, Links)
    output = processed_text;
    output = std::regex_replace(output, std::regex(R"(\*\*\*(.*?)\*\*\*)"), BOLD + ITALIC + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*\*(.*?)\*\*)"), BOLD + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*(.*?)\*)"), ITALIC + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(`([^`]+)`)"), GREEN + " $1 " + RESET);
    output = std::regex_replace(output, std::regex(R"(\[(.*?)\]\((.*?)\))"), BLUE + "$1" + RESET + " (" + CYAN + "$2" + RESET + ")");

    // 4. Restore Code Blocks
    for (int i = 0; i < block_idx; ++i) {
        std::string placeholder = placeholder_prefix + std::to_string(i) + "___";
        size_t pos = output.find(placeholder);
        if (pos != std::string::npos) {
            output.replace(pos, placeholder.length(), codeBlocks[i]);
        }
    }

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
#ifdef _WIN32
    // Force UTF-8 encoding for Windows terminal
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    // Enable Virtual Terminal Processing for ANSI colors on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }
#endif

    // Global exception handler to prevent silent crash
    try {
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
                // Already handled error printing in LLMClient
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
                    nlohmann::json args;
                    try {
                        args = nlohmann::json::parse(argsStr);
                    } catch (...) {
                        args = nlohmann::json::object();
                    }

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
                if (message.contains("content") && !message["content"].is_null()) {
                    std::string content = message["content"].get<std::string>();
                    std::cout << MAGENTA << BOLD << "Photon > " << RESET << renderMarkdown(content) << std::endl;
                }
                
                // Beautiful Context Usage Display
                size_t currentSize = contextManager.getSize(messages);
                std::cout << CYAN << "── Memory: " << BOLD << currentSize << RESET << CYAN << " chars ──" << RESET << std::endl;
                
                continues = false;
            }
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    } catch (const std::exception& e) {
        std::cerr << "\n" << RED << BOLD << "█ FATAL ERROR: " << RESET << e.what() << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "\n" << RED << BOLD << "█ UNKNOWN FATAL ERROR" << RESET << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
