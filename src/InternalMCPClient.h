#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdio>
#include "MCPClient.h"

namespace fs = std::filesystem;

class InternalMCPClient : public IMCPClient {
public:
    InternalMCPClient(const std::string& rootPath);
    
    nlohmann::json listTools() override;
    nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments) override;

private:
    std::string rootPath;

    nlohmann::json fileSearch(const nlohmann::json& args);
    nlohmann::json fileRead(const nlohmann::json& args);
    nlohmann::json fileWrite(const nlohmann::json& args);
    nlohmann::json pythonSandbox(const nlohmann::json& args);
    nlohmann::json bashExecute(const nlohmann::json& args);
    nlohmann::json codeAstAnalyze(const nlohmann::json& args);
    nlohmann::json gitOperations(const nlohmann::json& args);
    nlohmann::json webFetch(const nlohmann::json& args);
    nlohmann::json webSearch(const nlohmann::json& args);
    nlohmann::json harmonySearch(const nlohmann::json& args);
    
    std::string executeCommand(const std::string& cmd);
};
