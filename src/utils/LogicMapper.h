#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>
#include "utils/SymbolManager.h"
#include "mcp/LSPClient.h"

struct CallPoint {
    std::string name;
    int line;
    int character;
    std::string callerPath;
};

struct LogicNode {
    std::string id;
    std::string name;
    std::string path;
    int line;
    std::string type; // "function", "class", etc.
    std::string summary;
};

struct LogicEdge {
    std::string from;
    std::string to;
    std::string type; // "calls", "instantiates", etc.
};

class LogicMapper {
public:
    LogicMapper(SymbolManager& symbolManager, const std::unordered_map<std::string, LSPClient*>& lspClients, LSPClient* fallbackLSP);

    nlohmann::json generateMap(const std::string& entrySymbolName, int maxDepth = 3);

private:
    SymbolManager& symbolManager;
    const std::unordered_map<std::string, LSPClient*>& lspClients;
    LSPClient* fallbackLSP;

    struct Graph {
        std::vector<LogicNode> nodes;
        std::vector<LogicEdge> edges;
        std::unordered_map<std::string, int> nodeToIndex;
    };

    void crawl(const Symbol& current, int depth, int maxDepth, Graph& graph);
    std::vector<CallPoint> extractCalls(const Symbol& sym);
    LSPClient* getLSPForFile(const std::string& path);
    
    std::string getNodeId(const Symbol& sym);
};
