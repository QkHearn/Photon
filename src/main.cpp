#include <iostream>
#include <string>
#include <memory>
#include "FileManager.h"
#include "LLMClient.h"
#include "ContextManager.h"
#include "ConfigManager.h"
#include "MCPClient.h"
#include "MCPManager.h"

void printUsage() {
    std::cout << "Usage: photon <directory_path> [config_path]" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string path = argv[1];
    std::string configPath = (argc >= 3) ? argv[2] : "config.json";

    Config cfg;
    try {
        cfg = Config::load(configPath);
        std::cout << "Loaded configuration from: " << configPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        std::cerr << "Make sure " << configPath << " exists and is valid JSON." << std::endl;
        return 1;
    }

    auto llmClient = std::make_shared<LLMClient>(cfg.llm.apiKey, cfg.llm.baseUrl, cfg.llm.model);
    FileManager fileManager(path, cfg.agent.fileExtensions);
    ContextManager contextManager(llmClient, cfg.agent.contextThreshold);

    // Initialize MCP Manager and connect all servers
    MCPManager mcpManager;
    mcpManager.initFromConfig(cfg.mcpServers);
    
    // Get all available tools to inform the LLM
    auto allTools = mcpManager.getAllTools();
    if (!allTools.empty()) {
        std::string toolInfo = "\nAvailable MCP Tools:\n" + allTools.dump(2);
        contextManager.addContext("System: The following external tools are available via MCP servers:" + toolInfo);
    }

    std::cout << "--- Photon Agent ---" << std::endl;
    std::cout << "Scanning directory: " << path << std::endl;

    auto files = fileManager.readAllFiles();
    std::cout << "Found and read " << files.size() << " text files." << std::endl;

    // Initial context: list of files
    std::string initialInfo = "Available files in " + path + ":\n";
    for (const auto& [filePath, _] : files) {
        initialInfo += "- " + filePath + "\n";
    }
    contextManager.addContext(initialInfo);

    std::string userInput;
    while (true) {
        std::cout << "\nYou: ";
        if (!std::getline(std::cin, userInput) || userInput == "exit") break;

        if (userInput.empty()) continue;

        // Simple keyword search to pull relevant file content into context
        auto matchedFiles = fileManager.searchFiles(userInput);
        if (!matchedFiles.empty()) {
            std::string fileContext = "Relevant file contents for your query:\n";
            for (const auto& f : matchedFiles) {
                fileContext += "File: " + f + "\nContent Snippet:\n" + files[f].substr(0, 1000) + "\n---\n";
            }
            contextManager.addContext(fileContext);
        }

        // Generate response using current context
        std::string fullPrompt = "Context:\n" + contextManager.getContext() + "\n\nUser Question: " + userInput;
        std::string response = llmClient->chat(fullPrompt, cfg.llm.systemRole);

        std::cout << "\nAgent: " << response << std::endl;
        
        // Add response to context too
        contextManager.addContext("User asked: " + userInput + "\nAgent replied: " + response);
    }

    return 0;
}
