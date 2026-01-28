#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdio>
#include "mcp/MCPClient.h"

#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <thread>
#include <csignal> // For kill
#include "utils/SkillManager.h"
#include "utils/SymbolManager.h"
#include "mcp/LSPClient.h"

namespace fs = std::filesystem;

class InternalMCPClient : public IMCPClient {
public:
    InternalMCPClient(const std::string& rootPath);
    ~InternalMCPClient(); 
    
    void setSearchApiKey(const std::string& key) {
        this->searchApiKey = key;
    }
    
    void setSkillManager(SkillManager* mgr) {
        this->skillManager = mgr;
    }

    void setSymbolManager(SymbolManager* mgr) {
        this->symbolManager = mgr;
    }

    void setLSPClient(LSPClient* client) { this->lspClient = client; }
    void setLSPClients(const std::unordered_map<std::string, LSPClient*>& byExt, LSPClient* fallback) {
        lspByExtension = byExt;
        lspClient = fallback;
    }

    nlohmann::json listTools() override;
    nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments) override;

    std::string getLastFile() const override { return lastFile; }

private:
    fs::path rootPath;
    fs::path globalDataPath; // Global storage path (~/.photon or same dir as exe)
    std::string lastFile;
    bool isGitRepo = false;
    std::string searchApiKey;
    SkillManager* skillManager = nullptr;
    SymbolManager* symbolManager = nullptr;
    LSPClient* lspClient = nullptr;
    std::unordered_map<std::string, LSPClient*> lspByExtension;

    using ToolHandler = std::function<nlohmann::json(const nlohmann::json&)>;
    std::map<std::string, ToolHandler> toolHandlers;

    void registerTools();
    void syncKnowledgeIndex();
    nlohmann::json getProjectSummary(); // New: Get high-level overview
    bool checkGitRepo();
    bool isBinary(const fs::path& path);

    nlohmann::json fileSearch(const nlohmann::json& args);
    nlohmann::json fileRead(const nlohmann::json& args);
    nlohmann::json contextRead(const nlohmann::json& args);
    nlohmann::json diagnosticsCheck(const nlohmann::json& args);
    nlohmann::json fileWrite(const nlohmann::json& args);
    nlohmann::json pythonSandbox(const nlohmann::json& args);
    nlohmann::json pipInstall(const nlohmann::json& args);
    nlohmann::json sequentialThinking(const nlohmann::json& args);
    nlohmann::json archVisualize(const nlohmann::json& args);
    nlohmann::json bashExecute(const nlohmann::json& args);
    nlohmann::json codeAstAnalyze(const nlohmann::json& args);
    nlohmann::json gitOperations(const nlohmann::json& args);
    nlohmann::json webFetch(const nlohmann::json& args);
    nlohmann::json webSearch(const nlohmann::json& args);
    nlohmann::json harmonySearch(const nlohmann::json& args);
    nlohmann::json grepSearch(const nlohmann::json& args);
    nlohmann::json readFileLines(const nlohmann::json& args);
    nlohmann::json listDirTree(const nlohmann::json& args);
    nlohmann::json diffApply(const nlohmann::json& args);
    nlohmann::json fileEditLines(const nlohmann::json& args);
    nlohmann::json fileUndo(const nlohmann::json& args);
    nlohmann::json memoryStore(const nlohmann::json& args);
    nlohmann::json memoryList(const nlohmann::json& args);
    nlohmann::json memoryRetrieve(const nlohmann::json& args);
    nlohmann::json projectOverview(const nlohmann::json& args);
    nlohmann::json symbolSearch(const nlohmann::json& args);
    nlohmann::json lspDefinition(const nlohmann::json& args);
    nlohmann::json resolveRelativeDate(const nlohmann::json& args);
    nlohmann::json skillRead(const nlohmann::json& args);
    nlohmann::json osScheduler(const nlohmann::json& args);
    nlohmann::json listTasks(const nlohmann::json& args);
    nlohmann::json cancelTask(const nlohmann::json& args);
    
    std::string executeCommand(const std::string& cmd);
    
private:
    struct BackgroundTask {
        std::string id;
        std::string description;
        int pid;
        bool isPeriodic;
        int interval;
        std::time_t startTime;
    };
    std::vector<BackgroundTask> tasks;
    std::string generateTaskId();
    void killTaskProcess(int pid);
    bool shouldIgnore(const fs::path& path);
    std::string cleanHtml(const std::string& html);
    std::string htmlToMarkdown(const std::string& html);
    void backupFile(const std::string& relPath);
    void ensurePhotonDirs();
    bool isCommandSafe(const std::string& cmd);
    bool commandExists(const std::string& cmd);
    void saveTasksToDisk();
    void loadTasksFromDisk();
    std::string autoDetectBuildCommand();
};
