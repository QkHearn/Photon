#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdio>
#include "MCPClient.h"

#include <functional>
#include <map>
#include "SkillManager.h"

namespace fs = std::filesystem;

class InternalMCPClient : public IMCPClient {
public:
    InternalMCPClient(const std::string& rootPath);
    
    void setSearchApiKey(const std::string& key) {
        this->searchApiKey = key;
    }
    
    void setSkillManager(SkillManager* mgr) {
        this->skillManager = mgr;
    }

    nlohmann::json listTools() override;
    nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments) override;

    std::string getLastFile() const override { return lastFile; }

private:
    fs::path rootPath;
    std::string lastFile;
    bool isGitRepo = false;
    std::string searchApiKey;
    SkillManager* skillManager = nullptr;

    using ToolHandler = std::function<nlohmann::json(const nlohmann::json&)>;
    std::map<std::string, ToolHandler> toolHandlers;

    void registerTools();
    bool checkGitRepo();
    bool isBinary(const fs::path& path);

    nlohmann::json fileSearch(const nlohmann::json& args);
    nlohmann::json fileRead(const nlohmann::json& args);
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
    nlohmann::json fileUndo(const nlohmann::json& args);
    nlohmann::json memoryStore(const nlohmann::json& args);
    nlohmann::json memoryRetrieve(const nlohmann::json& args);
    nlohmann::json resolveRelativeDate(const nlohmann::json& args);
    nlohmann::json skillRead(const nlohmann::json& args);
    
    std::string executeCommand(const std::string& cmd);
    bool shouldIgnore(const fs::path& path);
    std::string cleanHtml(const std::string& html);
    std::string htmlToMarkdown(const std::string& html);
    void backupFile(const std::string& relPath);
    void ensurePhotonDirs();
    bool isCommandSafe(const std::string& cmd);
};
