#include "analysis/LogicMapper.h"
#include "analysis/providers/TreeSitterSymbolProvider.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

LogicMapper::LogicMapper(SymbolManager& symbolManager, const std::unordered_map<std::string, LSPClient*>& lspClients, LSPClient* fallbackLSP)
    : symbolManager(symbolManager), lspClients(lspClients), fallbackLSP(fallbackLSP) {}

nlohmann::json LogicMapper::generateMap(const std::string& entrySymbolName, int maxDepth) {
    Graph graph;
    
    // 1. Find the entry point
    std::vector<Symbol> entrySymbols = symbolManager.search(entrySymbolName);
    if (entrySymbols.empty()) {
        // Try exact match if search returns too many or none
        auto allSymbols = symbolManager.search("");
        for (const auto& s : allSymbols) {
            if (s.name == entrySymbolName && (s.type == "function" || s.type == "method")) {
                entrySymbols.push_back(s);
                break;
            }
        }
    }

    if (entrySymbols.empty()) {
        return {{"error", "Entry symbol not found: " + entrySymbolName}};
    }

    // Use the first one found (usually main)
    crawl(entrySymbols[0], 0, maxDepth, graph);

    nlohmann::json nodes = nlohmann::json::array();
    for (const auto& node : graph.nodes) {
        nodes.push_back({
            {"id", node.id},
            {"name", node.name},
            {"path", node.path},
            {"line", node.line},
            {"type", node.type},
            {"summary", node.summary}
        });
    }

    nlohmann::json edges = nlohmann::json::array();
    for (const auto& edge : graph.edges) {
        edges.push_back({
            {"from", edge.from},
            {"to", edge.to},
            {"type", edge.type}
        });
    }

    return {
        {"root", getNodeId(entrySymbols[0])},
        {"nodes", nodes},
        {"edges", edges}
    };
}

void LogicMapper::crawl(const Symbol& current, int depth, int maxDepth, Graph& graph) {
    std::string id = getNodeId(current);
    if (graph.nodeToIndex.count(id)) return;

    // Add node
    graph.nodeToIndex[id] = static_cast<int>(graph.nodes.size());
    graph.nodes.push_back({
        id, current.name, current.path, current.line, current.type, ""
    });

    if (depth >= maxDepth) return;

    // 2. Extract calls within this symbol
    std::vector<CallPoint> calls = extractCalls(current);

    for (const auto& cp : calls) {
        LSPClient* lsp = getLSPForFile(cp.callerPath);
        if (!lsp) continue;

        LSPClient::Position pos = {cp.line - 1, cp.character};
        std::string uri = "file://" + fs::absolute(cp.callerPath).u8string();
        
        auto locations = lsp->goToDefinition(uri, pos);
        if (locations.empty()) continue;

        // Find the symbol at this location in SymbolManager
        std::string targetPath = fs::relative(LSPClient::uriToPath(locations[0].uri), fs::current_path()).u8string();
        int targetLine = locations[0].range.start.line + 1;

        auto fileSymbols = symbolManager.getFileSymbols(targetPath);
        Symbol targetSymbol;
        bool found = false;
        for (const auto& s : fileSymbols) {
            if (s.line == targetLine || (targetLine >= s.line && (s.endLine == 0 || targetLine <= s.endLine))) {
                targetSymbol = s;
                found = true;
                break;
            }
        }

        if (found) {
            std::string targetId = getNodeId(targetSymbol);
            graph.edges.push_back({id, targetId, "calls"});
            crawl(targetSymbol, depth + 1, maxDepth, graph);
        }
    }
}

std::vector<CallPoint> LogicMapper::extractCalls(const Symbol& sym) {
    std::vector<CallPoint> calls;
    
    auto extracted = symbolManager.extractCalls(sym.path, sym.line, sym.endLine);
    for (const auto& c : extracted) {
        calls.push_back({c.name, c.line, c.character, sym.path});
    }
    
    return calls;
}

LSPClient* LogicMapper::getLSPForFile(const std::string& path) {
    std::string ext = fs::path(path).extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    auto it = lspClients.find(ext);
    if (it != lspClients.end()) return it->second;
    return fallbackLSP;
}

std::string LogicMapper::getNodeId(const Symbol& sym) {
    return sym.path + ":" + std::to_string(sym.line) + ":" + sym.name;
}
