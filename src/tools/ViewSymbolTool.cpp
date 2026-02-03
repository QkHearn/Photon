#include "ViewSymbolTool.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

ViewSymbolTool::ViewSymbolTool(SymbolManager* symbolMgr) 
    : symbolMgr(symbolMgr) {
    if (symbolMgr) {
        rootPath = symbolMgr->getRootPath();
    } else {
        rootPath = fs::current_path().string();
    }
}

std::string ViewSymbolTool::getName() const {
    return "view_symbol";
}

std::string ViewSymbolTool::getDescription() const {
    return "View the code of a specific symbol (function, class, method, etc.) in a file. "
           "Use this after the agent provides a symbol summary, instead of reading the entire file. "
           "This tool will return the exact line range and code content of the symbol.";
}

nlohmann::json ViewSymbolTool::getSchema() const {
    nlohmann::json schema;
    schema["type"] = "object";
    schema["required"] = nlohmann::json::array({"file_path", "symbol_name"});
    
    nlohmann::json properties;
    
    // file_path
    nlohmann::json filePath;
    filePath["type"] = "string";
    filePath["description"] = "Relative path to the file containing the symbol";
    properties["file_path"] = filePath;
    
    // symbol_name
    nlohmann::json symbolName;
    symbolName["type"] = "string";
    symbolName["description"] = "Name of the symbol to view (function name, class name, etc.)";
    properties["symbol_name"] = symbolName;
    
    schema["properties"] = properties;
    
    return schema;
}

nlohmann::json ViewSymbolTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    // 验证参数
    if (!args.contains("file_path") || !args.contains("symbol_name")) {
        result["error"] = "Missing required parameters: file_path and symbol_name";
        return result;
    }
    
    std::string filePath = args["file_path"].get<std::string>();
    std::string symbolName = args["symbol_name"].get<std::string>();
    
    if (!symbolMgr) {
        result["error"] = "SymbolManager not available";
        return result;
    }
    
    // 查找符号
    auto symbols = symbolMgr->getFileSymbols(filePath);
    const Symbol* targetSymbol = nullptr;
    
    for (const auto& sym : symbols) {
        if (sym.name == symbolName) {
            targetSymbol = &sym;
            break;
        }
    }
    
    if (!targetSymbol) {
        result["error"] = "Symbol '" + symbolName + "' not found in " + filePath;
        
        // 提供建议：列出可用符号
        if (!symbols.empty()) {
            std::ostringstream suggestion;
            suggestion << "Available symbols in this file:\n";
            for (size_t i = 0; i < std::min(symbols.size(), size_t(10)); ++i) {
                suggestion << "  - " << symbols[i].name << " (" << symbols[i].type << ")\n";
            }
            result["suggestion"] = suggestion.str();
        }
        
        return result;
    }
    
    // 读取文件内容
    fs::path fullPath = fs::path(rootPath) / fs::path(filePath);
    std::ifstream file(fullPath);
    
    if (!file.is_open()) {
        result["error"] = "Failed to open file: " + filePath;
        return result;
    }
    
    // 读取指定行范围
    std::ostringstream codeContent;
    std::string line;
    int currentLine = 1;
    int startLine = targetSymbol->line;
    int endLine = targetSymbol->endLine;
    
    while (std::getline(file, line)) {
        if (currentLine >= startLine && currentLine <= endLine) {
            codeContent << line << "\n";
        }
        if (currentLine > endLine) break;
        currentLine++;
    }
    
    file.close();
    
    // 构建成功响应
    nlohmann::json content = nlohmann::json::array();
    
    nlohmann::json textContent;
    textContent["type"] = "text";
    
    std::ostringstream response;
    response << "Symbol: " << targetSymbol->name << "\n";
    response << "Type: " << targetSymbol->type << "\n";
    response << "Location: " << filePath << ":" << startLine << "-" << endLine << "\n";
    if (!targetSymbol->signature.empty()) {
        response << "Signature: " << targetSymbol->signature << "\n";
    }
    response << "Source: " << targetSymbol->source << "\n\n";
    response << "Code:\n";
    response << "```\n";
    response << codeContent.str();
    response << "```\n";
    
    textContent["text"] = response.str();
    content.push_back(textContent);
    
    result["content"] = content;
    
    return result;
}
