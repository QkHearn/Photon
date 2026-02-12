#include "CoreTools.h"
#include "analysis/SymbolManager.h"
#include "utils/ScanIgnore.h"
#include <iostream>
#include <vector>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>
#include <regex>
#include <ctime>
#include <cctype>
#include <sstream>

// Windows compatibility
#ifdef _WIN32
    #define popen _popen
    #define pclose _pclose
#endif

// ============================================================================
// UTF-8 Utilities
// ============================================================================

namespace UTF8Utils {
    std::string sanitize(const std::string& input) {
        std::string output;
        output.reserve(input.size());
        
        size_t i = 0;
        while (i < input.size()) {
            unsigned char c = static_cast<unsigned char>(input[i]);
            
            // 单字节 ASCII (0x00-0x7F)
            if (c <= 0x7F) {
                output.push_back(static_cast<char>(c));
                i++;
            }
            // 2 字节序列 (0xC2-0xDF) - 注意: 0xC0-0xC1 是无效的
            else if (c >= 0xC2 && c <= 0xDF) {
                if (i + 1 < input.size()) {
                    unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                    if ((c1 & 0xC0) == 0x80) {
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        i += 2;
                        continue;
                    }
                }
                // 不完整或无效的序列
                output.push_back('?');
                i++;
            }
            // 3 字节序列 (0xE0-0xEF)
            else if (c >= 0xE0 && c <= 0xEF) {
                if (i + 2 < input.size()) {
                    unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                    unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
                    
                    // 验证续字节
                    if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
                        // 额外验证：避免过长编码
                        if (c == 0xE0 && c1 < 0xA0) {
                            output.push_back('?');
                            i += 3;  // 跳过整个无效序列（3 字节）
                            continue;
                        }
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        output.push_back(input[i + 2]);
                        i += 3;
                        continue;
                    }
                }
                // 不完整或无效的序列
                output.push_back('?');
                i++;
            }
            // 4 字节序列 (0xF0-0xF4)
            else if (c >= 0xF0 && c <= 0xF4) {
                if (i + 3 < input.size()) {
                    unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
                    unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
                    unsigned char c3 = static_cast<unsigned char>(input[i + 3]);
                    
                    // 验证续字节
                    if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                        // 额外验证：避免过长编码和超出 Unicode 范围
                        if ((c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 > 0x8F)) {
                            output.push_back('?');
                            i += 4;  // 跳过整个无效序列（4 字节）
                            continue;
                        }
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        output.push_back(input[i + 2]);
                        output.push_back(input[i + 3]);
                        i += 4;
                        continue;
                    }
                }
                // 不完整或无效的序列
                output.push_back('?');
                i++;
            }
            // 孤立的续字节 (0x80-0xBF) 或其他无效字节：直接跳过
            else {
                i++;
            }
        }
        
        return output;
    }
}

// ============================================================================
// ReadCodeBlockTool Implementation
// ============================================================================

ReadCodeBlockTool::ReadCodeBlockTool(const std::string& rootPath, SymbolManager* symbolMgr, bool enableDebug) 
    : rootPath(fs::u8path(rootPath)), symbolMgr(symbolMgr), enableDebug(enableDebug) {}

std::string ReadCodeBlockTool::getDescription() const {
    return "Read code from a file with intelligent strategies: "
           "(1) No parameters → returns symbol summary for code files; "
           "(2) symbol_name specified → returns that symbol's code plus call chain (Calls / Called by) when index is available; "
           "(3) start_line/end_line specified → returns those lines; "
           "(4) Otherwise → returns full file. "
           "Parameters: file_path (required), symbol_name (optional), start_line (optional), end_line (optional).";
}

nlohmann::json ReadCodeBlockTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Relative path to the file"}
            }},
            {"symbol_name", {
                {"type", "string"},
                {"description", "Name of a specific symbol (function, class, method) to read. If provided, only that symbol's code will be returned."}
            }},
            {"start_line", {
                {"type", "integer"},
                {"description", "Starting line number (1-indexed, optional). Use with end_line to read a specific range."}
            }},
            {"end_line", {
                {"type", "integer"},
                {"description", "Ending line number (1-indexed, optional). Use with start_line to read a specific range."}
            }}
        }},
        {"required", {"file_path"}}
    };
}

nlohmann::json ReadCodeBlockTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("file_path")) {
        result["error"] = "Missing required parameter: file_path";
        return result;
    }
    
    std::string filePath = args["file_path"].get<std::string>();
    
    // 智能路径处理: 支持相对路径和绝对路径
    fs::path inputPath = fs::u8path(filePath);
    fs::path fullPath;
    
    if (inputPath.is_absolute()) {
        fullPath = inputPath;
    } else {
        fullPath = rootPath / inputPath;
    }
    
    // 检查文件是否存在
    if (!fs::exists(fullPath)) {
        result["error"] = "File not found: " + filePath;
        return result;
    }
    
    if (!fs::is_regular_file(fullPath)) {
        result["error"] = "Not a regular file: " + filePath;
        return result;
    }
    
    // 智能策略选择
    bool hasSymbolName = args.contains("symbol_name") && !args["symbol_name"].is_null();
    bool hasLineRange = args.contains("start_line") || args.contains("end_line");
    
    // 策略 1: 指定了 symbol_name → 返回符号代码
    if (hasSymbolName) {
        std::string symbolName = args["symbol_name"].get<std::string>();
        return readSymbolCode(filePath, symbolName);
    }
    
    // 策略 2: 指定了行范围 → 返回指定行
    if (hasLineRange) {
        int startLine = args.value("start_line", 1);
        int endLine = args.value("end_line", -1);
        return readLineRange(filePath, startLine, endLine);
    }
    
    // 策略 3: 无参数 + 代码文件 + SymbolManager 可用 → 返回符号摘要
    if (symbolMgr && isCodeFile(filePath)) {
        auto summary = generateSymbolSummaryNonBlocking(filePath);
        if (!summary.contains("error")) {
            return summary;
        }
    }
    
    // 策略 4: 默认 → 返回全文
    return readFullFile(filePath);
}

// ============================================================================
// ReadCodeBlockTool - 辅助方法实现
// ============================================================================

bool ReadCodeBlockTool::isCodeFile(const std::string& filePath) const {
    // 支持的代码文件扩展名
    static const std::vector<std::string> codeExtensions = {
        ".cpp", ".h", ".hpp", ".cc", ".cxx", ".c",  // C/C++
        ".py",                                       // Python
        ".js", ".ts", ".jsx", ".tsx",               // JavaScript/TypeScript
        ".java",                                     // Java
        ".go",                                       // Go
        ".rs",                                       // Rust
        ".cs",                                       // C#
        ".rb",                                       // Ruby
        ".php",                                      // PHP
        ".swift",                                    // Swift
        ".kt", ".kts",                              // Kotlin
        ".ets"                                       // ArkTS
    };
    
    fs::path path(filePath);
    std::string ext = path.extension().string();
    
    return std::find(codeExtensions.begin(), codeExtensions.end(), ext) != codeExtensions.end();
}

nlohmann::json ReadCodeBlockTool::generateSymbolSummary(const std::string& filePath) {
    nlohmann::json result;
    
    if (!symbolMgr) {
        if (enableDebug) std::cout << "[ReadCodeBlock] SymbolManager not available" << std::endl;
        result["error"] = "SymbolManager not available";
        return result;
    }
    
    // 再次检查扫描状态（双重保险）
    if (symbolMgr->isScanning()) {
        if (enableDebug) std::cout << "[ReadCodeBlock] Scan started during symbol summary, aborting" << std::endl;
        result["error"] = "Scan in progress";
        return result;
    }
    
    // 规范化路径: 统一转换为相对于 rootPath 的路径
    std::string normalizedPath = filePath;
    fs::path inputPath = fs::u8path(filePath);
    
    // 获取 rootPath 的规范化绝对路径（解析 . 和 .. 等）
    fs::path rootAbsPath;
    try {
        // 优先使用 canonical，如果失败则使用 absolute
        rootAbsPath = fs::canonical(rootPath);
    } catch (...) {
        rootAbsPath = fs::absolute(rootPath);
    }
    
    // 计算文件的绝对路径
    fs::path absPath;
    if (inputPath.is_absolute()) {
        absPath = inputPath;
    } else {
        absPath = fs::absolute(rootAbsPath / inputPath);
    }
    
    // 调试信息 - 路径转换前（只在调试模式下）
    static bool enableDebugLog = std::getenv("PHOTON_DEBUG_READ") != nullptr;
    if (enableDebugLog) {
        if (enableDebug) std::cout << "[ReadCodeBlock] === Path Normalization Debug ===" << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] Original path: " << filePath << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] SymbolManager root: " << symbolMgr->getRootPath() << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] Root absolute path: " << rootAbsPath.string() << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] File absolute path: " << absPath.string() << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] Is input absolute? " << (inputPath.is_absolute() ? "yes" : "no") << std::endl;
    }
    
    // 如果文件在 rootPath 下,计算相对路径
    try {
        // 使用 lexically_relative 或 relative 来计算相对路径
        auto relPath = absPath.lexically_relative(rootAbsPath);
        if (!relPath.empty() && relPath.string() != ".." && relPath.string().find("..") != 0) {
            // 使用 generic_string() 确保路径分隔符一致 (统一为 '/')
            normalizedPath = relPath.generic_string();
            if (enableDebugLog) {
                if (enableDebug) std::cout << "[ReadCodeBlock] Path is under root, converted to relative: " << normalizedPath << std::endl;
            }
        } else {
            if (enableDebugLog) {
                if (enableDebug) std::cout << "[ReadCodeBlock] Path is NOT under root, keeping original" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        // 如果无法计算相对路径,保持原样
        normalizedPath = filePath;
        if (enableDebugLog) {
            if (enableDebug) std::cout << "[ReadCodeBlock] Failed to compute relative path: " << e.what() << std::endl;
        }
    }
    
    if (enableDebugLog) {
        if (enableDebug) std::cout << "[ReadCodeBlock] Final normalized path: " << normalizedPath << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] Total symbols in index: " << symbolMgr->getSymbolCount() << std::endl;
        if (enableDebug) std::cout << "[ReadCodeBlock] Is scanning: " << (symbolMgr->isScanning() ? "yes" : "no") << std::endl;
    }
    
    auto symbols = symbolMgr->getFileSymbols(normalizedPath);
    
    if (enableDebugLog) {
        if (enableDebug) std::cout << "[ReadCodeBlock] Query for '" << normalizedPath << "' returned " << symbols.size() << " symbols" << std::endl;
        
        // 如果没找到，尝试列出索引中的文件路径样本
        if (symbols.empty()) {
            auto allSymbols = symbolMgr->search("");  // 获取所有符号
            std::set<std::string> uniquePaths;
            for (const auto& sym : allSymbols) {
                uniquePaths.insert(sym.path);
                if (uniquePaths.size() >= 10) break;
            }
            if (!uniquePaths.empty()) {
                if (enableDebug) std::cout << "[ReadCodeBlock] Sample paths in index:" << std::endl;
                for (const auto& p : uniquePaths) {
                    if (enableDebug) std::cout << "[ReadCodeBlock]   - '" << p << "'" << std::endl;
                }
            }
        }
    }
    
    // 如果索引中没有符号,检查是否可以实时分析
    if (symbols.empty()) {
        if (enableDebugLog) {
            if (enableDebug) std::cout << "[ReadCodeBlock] No symbols in index" << std::endl;
        }
        
        // 尝试找到实际文件路径
        fs::path actualPath;
        if (fs::exists(rootPath / fs::u8path(filePath))) {
            actualPath = rootPath / fs::u8path(filePath);
        } else if (fs::exists(fs::u8path(filePath))) {
            actualPath = fs::u8path(filePath);
        }
        
        // 如果文件存在且是代码文件,提示可以实时分析
        if (!actualPath.empty() && isCodeFile(filePath)) {
            if (enableDebugLog) {
                if (enableDebug) std::cout << "[ReadCodeBlock] File exists but not in index" << std::endl;
                if (enableDebug) std::cout << "[ReadCodeBlock] This might be:" << std::endl;
                if (enableDebug) std::cout << "[ReadCodeBlock]   1. A file outside the project" << std::endl;
                if (enableDebug) std::cout << "[ReadCodeBlock]   2. A newly created file" << std::endl;
                if (enableDebug) std::cout << "[ReadCodeBlock]   3. An ignored file" << std::endl;
                if (enableDebug) std::cout << "[ReadCodeBlock] Falling back to full file read" << std::endl;
            }
            
            // TODO: 未来可以实现临时符号提取
            // symbols = extractSymbolsOnDemand(actualPath);
        } else {
            if (enableDebugLog) {
                if (enableDebug) std::cout << "[ReadCodeBlock] File not found or not a code file" << std::endl;
            }
        }
        
        result["error"] = "No symbols found in file";
        return result;
    }
    
    if (enableDebug) std::cout << "[ReadCodeBlock] Found " << symbols.size() << " symbols" << std::endl;
    
    // 按类型分组
    std::map<std::string, std::vector<const Symbol*>> grouped;
    for (const auto& sym : symbols) {
        grouped[sym.type].push_back(&sym);
    }
    
    // 格式化符号摘要
    std::ostringstream summary;
    summary << "📊 Symbol Summary for: " << filePath << "\n\n";
    
    int totalSymbols = 0;
    for (const auto& [type, syms] : grouped) {
        if (syms.empty()) continue;
        
        summary << "### " << type << "s (" << syms.size() << "):\n";
        
        for (const auto* sym : syms) {
            summary << "  - `" << sym->name << "`";
            if (!sym->signature.empty() && sym->signature != sym->name) {
                summary << " - " << sym->signature;
            }
            summary << " (lines " << sym->line << "-" << sym->endLine << ")";
            summary << " [" << sym->source << "]\n";
            totalSymbols++;
            
            // 限制每个类型最多显示 20 个
            if (totalSymbols >= 20) break;
        }
        
        if (totalSymbols >= 20) {
            summary << "  ... (truncated, " << (symbols.size() - totalSymbols) << " more symbols)\n";
            break;
        }
    }
    
    summary << "\n💡 **Next Steps**:\n";
    summary << "  - Use `read_code_block` with `symbol_name` to view specific symbols\n";
    summary << "  - Use `view_symbol` tool for detailed symbol information\n";
    summary << "  - Use `read_code_block` with `start_line`/`end_line` for specific ranges\n";
    
    // 返回格式化的摘要（清理 UTF-8 避免 JSON 报错）
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = UTF8Utils::sanitize(summary.str());
    
    result["content"] = nlohmann::json::array({contentItem});
    result["summary_mode"] = true;
    result["symbol_count"] = static_cast<int>(symbols.size());
    
    return result;
}

nlohmann::json ReadCodeBlockTool::generateSymbolSummaryNonBlocking(const std::string& filePath) {
    nlohmann::json result;
    
    if (!symbolMgr) {
        result["error"] = "SymbolManager not available";
        return result;
    }
    
    // 规范化路径
    std::string normalizedPath = filePath;
    fs::path inputPath = fs::u8path(filePath);
    
    fs::path rootAbsPath;
    try {
        rootAbsPath = fs::canonical(rootPath);
    } catch (...) {
        rootAbsPath = fs::absolute(rootPath);
    }
    
    fs::path absPath;
    if (inputPath.is_absolute()) {
        absPath = inputPath;
    } else {
        absPath = fs::absolute(rootAbsPath / inputPath);
    }
    
    try {
        auto relPath = absPath.lexically_relative(rootAbsPath);
        if (!relPath.empty() && relPath.string() != ".." && relPath.string().find("..") != 0) {
            normalizedPath = relPath.generic_string();
        }
    } catch (...) {
        normalizedPath = filePath;
    }
    
    if (enableDebug) std::cout << "[ReadCodeBlock] Normalized path: " << normalizedPath << std::endl;
    
    // 使用非阻塞查询
    std::vector<SymbolManager::Symbol> symbols;
    if (enableDebug) std::cout << "[ReadCodeBlock] Calling tryGetFileSymbols..." << std::endl;
    
    if (!symbolMgr->tryGetFileSymbols(normalizedPath, symbols)) {
        if (enableDebug) std::cout << "[ReadCodeBlock] tryGetFileSymbols failed (lock unavailable or not found)" << std::endl;
        result["error"] = "Lock unavailable or file not in index";
        return result;
    }
    
    if (enableDebug) std::cout << "[ReadCodeBlock] tryGetFileSymbols succeeded, got " << symbols.size() << " symbols" << std::endl;
    
    if (symbols.empty()) {
        result["error"] = "No symbols found";
        return result;
    }
    
    // 按类型分组
    std::map<std::string, std::vector<const SymbolManager::Symbol*>> grouped;
    for (const auto& sym : symbols) {
        grouped[sym.type].push_back(&sym);
    }
    
    // 格式化符号摘要
    std::ostringstream summary;
    summary << "📊 Symbol Summary for: " << filePath << "\n\n";
    
    int totalSymbols = 0;
    for (const auto& [type, syms] : grouped) {
        if (syms.empty()) continue;
        summary << "### " << type << "s (" << syms.size() << "):\n";
        for (const auto* sym : syms) {
            summary << "  - `" << sym->name << "`";
            if (!sym->signature.empty()) {
                summary << " - " << sym->signature;
            }
            summary << " (lines " << sym->line << "-" << sym->endLine << ")";
            summary << " [" << sym->source << "]\n";
            totalSymbols++;
            if (totalSymbols >= 20) break;
        }
        if (totalSymbols >= 20) break;
    }
    
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = UTF8Utils::sanitize(summary.str());
    
    result["content"] = nlohmann::json::array({contentItem});
    result["summary_mode"] = true;
    result["symbol_count"] = static_cast<int>(symbols.size());
    
    return result;
}

nlohmann::json ReadCodeBlockTool::readSymbolCode(const std::string& filePath, const std::string& symbolName) {
    nlohmann::json result;
    
    if (!symbolMgr) {
        result["error"] = "SymbolManager not available";
        return result;
    }
    
    // 规范化路径: 统一转换为相对于 rootPath 的路径
    std::string normalizedPath = filePath;
    fs::path inputPath = fs::u8path(filePath);
    
    // 获取 rootPath 的规范化绝对路径（解析 . 和 .. 等）
    fs::path rootAbsPath;
    try {
        rootAbsPath = fs::canonical(rootPath);
    } catch (...) {
        rootAbsPath = fs::absolute(rootPath);
    }
    
    // 计算文件的绝对路径
    fs::path absPath;
    if (inputPath.is_absolute()) {
        absPath = inputPath;
    } else {
        absPath = fs::absolute(rootAbsPath / inputPath);
    }
    
    // 如果文件在 rootPath 下,计算相对路径
    try {
        auto relPath = absPath.lexically_relative(rootAbsPath);
        if (!relPath.empty() && relPath.string() != ".." && relPath.string().find("..") != 0) {
            // 使用 generic_string() 确保路径分隔符一致 (统一为 '/')
            normalizedPath = relPath.generic_string();
        }
    } catch (...) {
        normalizedPath = filePath;
    }
    
    // 查找符号
    auto symbols = symbolMgr->getFileSymbols(normalizedPath);
    const Symbol* targetSymbol = nullptr;
    
    for (const auto& sym : symbols) {
        if (sym.name == symbolName) {
            targetSymbol = &sym;
            break;
        }
    }
    
    if (!targetSymbol) {
        result["error"] = "Symbol '" + symbolName + "' not found in " + filePath;
        
        // 提供建议
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
    
    // 读取符号对应的行范围
    result = readLineRange(filePath, targetSymbol->line, targetSymbol->endLine);
    if (result.contains("error")) return result;

    // 附加调用链信息（Calls / Called by），便于分析调用关系
    auto callees = symbolMgr->getCalleesForSymbol(*targetSymbol);
    auto callers = symbolMgr->getCallerKeysForSymbol(*targetSymbol);
    if (!callees.empty() || !callers.empty()) {
        std::ostringstream chain;
        chain << "\n\n--- Call chain ---\n";
        auto formatKey = [](const std::string& k) -> std::string {
            size_t last = k.rfind(':');
            if (last == std::string::npos || last == 0) return k;
            size_t prev = k.rfind(':', last - 1);
            if (prev == std::string::npos) return k;
            return k.substr(0, prev) + "::" + k.substr(last + 1);
        };
        if (!callees.empty()) {
            chain << "Calls: ";
            for (size_t i = 0; i < callees.size(); ++i) {
                if (i > 0) chain << ", ";
                chain << formatKey(callees[i]);
            }
            chain << "\n";
        }
        if (!callers.empty()) {
            chain << "Called by: ";
            for (size_t i = 0; i < callers.size(); ++i) {
                if (i > 0) chain << ", ";
                chain << formatKey(callers[i]);
            }
            chain << "\n";
        }
        std::string append = chain.str();
        if (result.contains("content") && result["content"].is_array() && !result["content"].empty()
            && result["content"][0].contains("text") && result["content"][0]["text"].is_string()) {
            std::string existing = result["content"][0]["text"].get<std::string>();
            result["content"][0]["text"] = existing + UTF8Utils::sanitize(append);
        }
    }
    return result;
}

nlohmann::json ReadCodeBlockTool::readLineRange(const std::string& filePath, int startLine, int endLine) {
    nlohmann::json result;
    
    // 智能路径处理: 支持相对路径和绝对路径
    fs::path inputPath = fs::u8path(filePath);
    fs::path fullPath;
    
    if (inputPath.is_absolute()) {
        fullPath = inputPath;
    } else {
        fullPath = rootPath / inputPath;
    }
    
    // 读取文件（二进制模式以处理编码问题）
    if (enableDebug) std::cout << "[ReadCodeBlock] Opening file: " << fullPath.string() << std::endl;
    
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        result["error"] = "Failed to open file: " + filePath;
        return result;
    }
    
    if (enableDebug) std::cout << "[ReadCodeBlock] Reading lines..." << std::endl;
    
    std::vector<std::string> lines;
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (enableDebug && lineNum <= 3) {
            std::cout << "[ReadCodeBlock] Line " << lineNum << " length: " << line.size() << std::endl;
        }
        lines.push_back(line);
    }
    file.close();
    
    if (enableDebug) std::cout << "[ReadCodeBlock] Read " << lines.size() << " lines" << std::endl;
    
    int totalLines = static_cast<int>(lines.size());
    
    // 如果 endLine 未指定或为 -1,使用总行数
    if (endLine == -1) {
        endLine = totalLines;
    }
    
    // 边界检查
    if (startLine < 1) startLine = 1;
    if (endLine > totalLines) endLine = totalLines;
    if (startLine > endLine) {
        result["error"] = "Invalid range: start_line > end_line";
        return result;
    }
    
    // 构建内容
    if (enableDebug) std::cout << "[ReadCodeBlock] Building content for lines " << startLine << "-" << endLine << std::endl;
    
    std::ostringstream content;
    for (int i = startLine - 1; i < endLine; ++i) {
        content << (i + 1) << "|" << lines[i];
        if (i < endLine - 1) content << "\n";
    }
    
    // 构建最终内容
    std::string finalContent = "File: " + filePath + "\n" +
                               "Lines: " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                               " (Total: " + std::to_string(totalLines) + ")\n\n" +
                               content.str();
    
    if (enableDebug) {
        std::cout << "[ReadCodeBlock] Final content size: " << finalContent.size() << std::endl;
    }
    
    // 始终清理无效 UTF-8
    std::string cleanContent = UTF8Utils::sanitize(finalContent);
    
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    try {
        contentItem["text"] = cleanContent;
    } catch (const std::exception& e) {
        // sanitize 后仍可能触发校验：用仅 ASCII 兜底，保证不崩溃
        std::string safe;
        safe.reserve(cleanContent.size());
        for (unsigned char c : cleanContent) {
            if (c < 0x80) safe.push_back(c);
            else safe.push_back('?');
        }
        contentItem["text"] = safe;
    }
    result["content"] = nlohmann::json::array({contentItem});
    
    return result;
}

nlohmann::json ReadCodeBlockTool::readFullFile(const std::string& filePath) {
    // 直接调用 readLineRange 读取全文
    return readLineRange(filePath, 1, -1);
}

// ============================================================================
// ApplyPatchTool Implementation
// ============================================================================

ApplyPatchTool::ApplyPatchTool(const std::string& rootPath, bool hasGit) 
    : rootPath(fs::u8path(rootPath)), hasGit(hasGit) {}

std::string ApplyPatchTool::getDescription() const {
    std::string desc = "Modify or create project files by applying a unified diff (recommended for all file edits: reversible, trackable). "
                      "Provide diff_content: each line added with '+' prefix, removed with '-' prefix, unchanged with space. "
                      "Include at least one hunk header (e.g. \"@@ -1,3 +1,4 @@\" for edits, \"@@ -0,0 +1,N @@\" for new files). "
                      "New file: use \"--- /dev/null\" and \"+++ b/path/to/newfile\" with only '+' lines. "
                      "Multiple files: write one block per file (diff --git / --- / +++ / @@ hunks), then immediately the next block with no blank line between files; inside each hunk every line must start with ' ', '+', or '-' (no empty lines in hunks, no trailing spaces). Use dry_run: true for complex or multi-file diffs first. ";
    
    if (hasGit) {
        desc += "Uses git stash for backup and git apply when available.";
    } else {
        desc += "Pure diff mode with file-level backups.";
    }
    
    return desc;
}

nlohmann::json ApplyPatchTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"diff_content", {
                {"type", "string"},
                {"description", "Unified diff string. Rules: (1) Every line inside a hunk must start with exactly one of: space ' ', '+', or '-'; no empty lines inside hunks. (2) No trailing spaces on any line. (3) Single file: one block with ---/+++/@@ and hunk lines. (4) Multiple files: for each file write 'diff --git a/path b/path', then '--- a/path' (or '--- /dev/null' for new file), then '+++ b/path', then '@@ -... +... @@' and hunk lines; start the next file's 'diff --git' or '--- /dev/null' on the very next line after the last hunk line (no blank line between files). New file: --- /dev/null, +++ b/path, @@ -0,0 +1,N @@, then only '+' lines."}
            }},
            {"files", {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", "Optional: specific files to apply diff to. If not provided, applies to all files in diff."}
            }},
            {"backup", {
                {"type", "boolean"},
                {"description", "Whether to create backup before applying diff (default: true)"}
            }},
            {"dry_run", {
                {"type", "boolean"},
                {"description", "If true, only validate diff (git apply --check), do not write. Recommend setting true first for complex patches to avoid apply errors (default: false)."}
            }}
        }},
        {"required", {"diff_content"}}
    };
}

static fs::path sanitizePathComponent(fs::path p) {
    // Keep it simple and filesystem-friendly.
    std::string s = p.u8string();
    for (auto& ch : s) {
        if (ch == ':' || ch == '\\') ch = '_';
    }
    return fs::u8path(s);
}

static fs::path backupRelativePathFor(const fs::path& srcPath, const fs::path& rootPath) {
    if (!srcPath.is_absolute()) {
        return srcPath;
    }

    // If the file lives under project root, back it up by its project-relative path.
    std::error_code ec;
    fs::path rel = fs::relative(srcPath, rootPath, ec);
    if (!ec && !rel.empty()) {
        // Avoid paths that escape the root (../..)
        std::string relStr = rel.u8string();
        if (relStr.rfind("..", 0) != 0) {
            return rel;
        }
    }

    // External absolute path: map into backups/abs/...
    fs::path rn = srcPath.root_name();         // e.g. "C:" on Windows
    fs::path rp = srcPath.relative_path();     // drops root dir, e.g. "/a/b" -> "a/b"
    if (!rn.empty()) {
        return fs::path("abs") / sanitizePathComponent(rn) / rp;
    }
    return fs::path("abs") / rp;
}

bool ApplyPatchTool::createGitBackup(const std::string& path) {
    try {
        fs::path rawPath = fs::u8path(path);
        fs::path srcPath = rawPath.is_absolute() ? rawPath : (rootPath / rawPath);
        
        // 确保文件已被Git跟踪
        std::string gitCheckCmd =
#ifdef _WIN32
            "git ls-files --error-unmatch \"" + srcPath.u8string() + "\" >nul 2>nul";
#else
            "git ls-files --error-unmatch \"" + srcPath.u8string() + "\" >/dev/null 2>&1";
#endif
        int result = system(gitCheckCmd.c_str());
        if (result != 0) {
            return false; // 文件未被Git跟踪，不能使用Git备份
        }
        
        // 使用Git stash创建备份
        std::string stashCmd =
#ifdef _WIN32
            "cd \"" + rootPath.u8string() + "\" && git stash push -m \"photon-backup-" + path + "\" -- \"" + srcPath.u8string() + "\" >nul 2>nul";
#else
            "cd \"" + rootPath.u8string() + "\" && git stash push -m \"photon-backup-" + path + "\" -- \"" + srcPath.u8string() + "\" >/dev/null 2>&1";
#endif
        result = system(stashCmd.c_str());
        return (result == 0);
    } catch (const std::exception& e) {
        return false;
    }
}

void ApplyPatchTool::createLocalBackup(const std::string& path) {
    fs::path backupDir = rootPath / ".photon" / "backups";
    fs::create_directories(backupDir);
    
    fs::path rawPath = fs::u8path(path);
    fs::path srcPath = rawPath.is_absolute() ? rawPath : (rootPath / rawPath);

    fs::path rel = backupRelativePathFor(srcPath, rootPath);
    fs::path dstPath = backupDir / rel;
    
    fs::create_directories(dstPath.parent_path());
    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);
}

void ApplyPatchTool::createBackup(const std::string& path) {
    if (hasGit) {
        // 优先尝试Git备份
        if (createGitBackup(path)) {
            return; // Git备份成功
        }
    }
    
    // 回退到本地备份
    createLocalBackup(path);
}

bool ApplyPatchTool::writeFileWithGit(const std::string& path, const std::vector<std::string>& lines) {
    try {
        fs::path rawPath = fs::u8path(path);
        fs::path fullPath = rawPath.is_absolute() ? rawPath : (rootPath / rawPath);
        
        // 确保文件已被Git跟踪
        std::string gitCheckCmd =
#ifdef _WIN32
            "git ls-files --error-unmatch \"" + fullPath.u8string() + "\" >nul 2>nul";
#else
            "git ls-files --error-unmatch \"" + fullPath.u8string() + "\" >/dev/null 2>&1";
#endif
        int result = system(gitCheckCmd.c_str());
        if (result != 0) {
            return false; // 文件未被Git跟踪，不能使用Git写入
        }
        
        // 写回文件
        std::ofstream outFile(fullPath);
        if (!outFile.is_open()) {
            return false;
        }
        
        for (size_t i = 0; i < lines.size(); ++i) {
            outFile << lines[i];
            if (i < lines.size() - 1) outFile << "\n";
        }
        outFile.close();
        
        // 使用Git添加更改
        std::string gitAddCmd =
#ifdef _WIN32
            "cd \"" + rootPath.u8string() + "\" && git add \"" + fullPath.u8string() + "\" >nul 2>nul";
#else
            "cd \"" + rootPath.u8string() + "\" && git add \"" + fullPath.u8string() + "\" >/dev/null 2>&1";
#endif
        result = system(gitAddCmd.c_str());
        return (result == 0);
    } catch (const std::exception& e) {
        return false;
    }
}

static int execCaptureCoreTools(const std::string& cmd, std::string& out) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return -1;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

static std::string stripGitPrefix(const std::string& p) {
    if (p.rfind("a/", 0) == 0 || p.rfind("b/", 0) == 0) return p.substr(2);
    return p;
}

// Normalize diff so git apply accepts it: strip trailing whitespace per line, and replace
// empty lines inside hunks with " " (unified diff requires every hunk line to start with ' ', '+', or '-').
static std::string normalizePatchForGitApply(const std::string& diffContent) {
    std::string out;
    out.reserve(diffContent.size());
    bool inHunk = false;
    size_t i = 0;
    while (i < diffContent.size()) {
        size_t lineStart = i;
        while (i < diffContent.size() && diffContent[i] != '\n' && diffContent[i] != '\r') ++i;
        std::string line(diffContent.substr(lineStart, i - lineStart));
        if (i < diffContent.size() && diffContent[i] == '\r') ++i;
        if (i < diffContent.size() && diffContent[i] == '\n') ++i;
        // Strip trailing spaces/tabs
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (inHunk && line.empty()) line = " ";
        if (line.size() >= 3 && line.compare(0, 3, "@@ ") == 0 &&
            line.find(" @@") != std::string::npos) inHunk = true;
        if (line.rfind("--- ", 0) == 0 || line.rfind("diff --git ", 0) == 0) inHunk = false;
        if (inHunk && !line.empty() && line[0] != ' ' && line[0] != '+' && line[0] != '-') inHunk = false;
        out += line;
        out += '\n';
    }
    return out;
}

static bool isDevNull(const std::string& p) {
    return p == "/dev/null" || p == "NUL";
}

std::vector<ApplyPatchTool::FileDiff> ApplyPatchTool::parseUnifiedDiff(const std::string& diffContent) {
    std::vector<FileDiff> files;
    FileDiff current{};
    bool haveCurrent = false;
    DiffHunk* activeHunk = nullptr;

    std::regex hunkRe(R"(^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@)");
    std::istringstream iss(diffContent);
    std::string line;

    auto flush = [&]() {
        if (!haveCurrent) return;
        current.oldFile = stripGitPrefix(current.oldFile);
        current.newFile = stripGitPrefix(current.newFile);
        files.push_back(std::move(current));
        current = FileDiff{};
        haveCurrent = false;
        activeHunk = nullptr;
    };

    while (std::getline(iss, line)) {
        if (line.rfind("diff --git ", 0) == 0) {
            flush();
            haveCurrent = true;
            current.isNewFile = false;
            current.isDeleted = false;
            current.hunks.clear();
            activeHunk = nullptr;

            // diff --git a/foo b/foo
            std::istringstream ds(line);
            std::string tmp, aPath, bPath;
            ds >> tmp >> tmp >> aPath >> bPath;
            current.oldFile = aPath;
            current.newFile = bPath;
            continue;
        }

        // Tolerate diffs without "diff --git" header
        if (!haveCurrent) {
            if (line.rfind("--- ", 0) == 0 || line.rfind("+++ ", 0) == 0) {
                haveCurrent = true;
                current.isNewFile = false;
                current.isDeleted = false;
                activeHunk = nullptr;
            } else {
                continue;
            }
        }

        if (line.rfind("new file mode ", 0) == 0) {
            current.isNewFile = true;
            continue;
        }
        if (line.rfind("deleted file mode ", 0) == 0) {
            current.isDeleted = true;
            continue;
        }

        if (line.rfind("--- ", 0) == 0) {
            std::string p = line.substr(4);
            if (!isDevNull(p)) current.oldFile = p;
            continue;
        }
        if (line.rfind("+++ ", 0) == 0) {
            std::string p = line.substr(4);
            if (!isDevNull(p)) current.newFile = p;
            continue;
        }

        std::smatch m;
        if (std::regex_search(line, m, hunkRe)) {
            DiffHunk h{};
            h.oldStart = std::stoi(m[1].str());
            h.oldCount = m[2].matched ? std::stoi(m[2].str()) : 1;
            h.newStart = std::stoi(m[3].str());
            h.newCount = m[4].matched ? std::stoi(m[4].str()) : 1;
            current.hunks.push_back(std::move(h));
            activeHunk = &current.hunks.back();
            continue;
        }

        if (activeHunk) {
            if (!line.empty() && (line[0] == ' ' || line[0] == '+' || line[0] == '-')) {
                activeHunk->lines.push_back(line);
            } else if (line.rfind("\\ No newline at end of file", 0) == 0) {
                // ignore
            }
        }
    }

    flush();
    return files;
}

bool ApplyPatchTool::applyFileChanges(const FileDiff& fileDiff, std::string* applyError) {
    std::string rel = fileDiff.isDeleted ? fileDiff.oldFile : fileDiff.newFile;
    if (rel.empty()) rel = fileDiff.oldFile;
    rel = stripGitPrefix(rel);
    if (rel.empty()) {
        if (applyError) *applyError = "补丁中未解析到有效文件路径。";
        return false;
    }

    fs::path full = fs::u8path(rel);
    if (!full.is_absolute()) full = rootPath / full;

    if (fileDiff.isDeleted) {
        if (!fs::exists(full)) return true;
        std::error_code ec;
        fs::remove(full, ec);
        if (ec && applyError) *applyError = rel + ": 删除文件失败。";
        return !ec;
    }

    std::vector<std::string> original;
    if (fs::exists(full)) {
        std::ifstream in(full);
        if (!in.is_open()) {
            if (applyError) *applyError = rel + ": 无法打开文件读取。";
            return false;
        }
        std::string l;
        while (std::getline(in, l)) original.push_back(l);
    }

    std::vector<std::string> out;
    size_t oldIdx = 0;

    for (const auto& h : fileDiff.hunks) {
        size_t targetOld0 = (h.oldStart <= 0) ? 0 : static_cast<size_t>(h.oldStart - 1);
        if (targetOld0 > original.size()) {
            if (applyError) *applyError = rel + ": 上下文不匹配（补丁期望从第 " + std::to_string(h.oldStart) + " 行开始，当前文件仅 " + std::to_string(original.size()) + " 行）。可能 diff 是针对其他项目/版本生成的。";
            return false;
        }
        while (oldIdx < targetOld0) out.push_back(original[oldIdx++]);

        for (const auto& hl : h.lines) {
            if (hl.empty()) continue;
            char prefix = hl[0];
            std::string content = hl.substr(1);
            if (prefix == ' ') {
                if (oldIdx >= original.size() || original[oldIdx] != content) {
                    if (applyError) {
                        size_t lineNo = oldIdx + 1;
                        std::string expected = content.length() > 40 ? content.substr(0, 37) + "..." : content;
                        std::string actual = (oldIdx < original.size() ? original[oldIdx] : "").length() > 40 ? original[oldIdx].substr(0, 37) + "..." : (oldIdx < original.size() ? original[oldIdx] : "(文件已结束)");
                        *applyError = rel + " 第 " + std::to_string(lineNo) + " 行: 上下文不匹配（预期与当前文件不一致）。可能 diff 是针对其他项目/版本生成的。";
                    }
                    return false;
                }
                out.push_back(content);
                oldIdx++;
            } else if (prefix == '-') {
                if (oldIdx >= original.size() || original[oldIdx] != content) {
                    if (applyError) *applyError = rel + " 第 " + std::to_string(oldIdx + 1) + " 行: 待删除行与当前文件不一致，上下文不匹配。可能 diff 是针对其他项目/版本生成的。";
                    return false;
                }
                oldIdx++;
            } else if (prefix == '+') {
                out.push_back(content);
            }
        }
        // 新增文件型 hunk（oldCount==0）：不再追加原文件剩余内容，避免「补丁内容+整份原文件」重复
        if (h.oldCount == 0)
            oldIdx = original.size();
    }

    while (oldIdx < original.size()) out.push_back(original[oldIdx++]);

    fs::create_directories(full.parent_path());
    std::ofstream of(full);
    if (!of.is_open()) {
        if (applyError) *applyError = rel + ": 无法写入文件（无权限或路径不可用）。";
        return false;
    }
    for (size_t i = 0; i < out.size(); ++i) {
        of << out[i];
        if (i + 1 < out.size()) of << "\n";
    }
    return true;
}

bool ApplyPatchTool::applyFileDiff(const std::string& filePath, const std::string& diffContent) {
    auto diffs = parseUnifiedDiff(diffContent);
    if (diffs.empty()) return false;
    std::string want = stripGitPrefix(filePath);
    for (const auto& fd : diffs) {
        std::string got = stripGitPrefix(fd.isDeleted ? fd.oldFile : fd.newFile);
        if (!want.empty() && !got.empty() && (want == got || want == stripGitPrefix(fd.oldFile))) {
            return applyFileChanges(fd);
        }
    }
    return applyFileChanges(diffs.front());
}

bool ApplyPatchTool::applyUnifiedDiff(const std::string& diffContent, std::string* applyError) {
    auto diffs = parseUnifiedDiff(diffContent);
    if (diffs.empty()) return false;
    for (const auto& fd : diffs) {
        if (!applyFileChanges(fd, applyError)) return false;
    }
    return true;
}

std::string ApplyPatchTool::generateUnifiedDiff(const std::vector<std::string>& /*files*/) {
    // apply_patch 只负责应用 diff；生成 diff 交由 git diff / 外部完成
    return "";
}

nlohmann::json ApplyPatchTool::execute(const nlohmann::json& args) {
    nlohmann::json result;

    if (!args.contains("diff_content") || !args["diff_content"].is_string()) {
        result["error"] = "apply_patch 只接受 unified diff 更新。请提供参数 diff_content。";
        return result;
    }

    const std::string diffContent = args["diff_content"].get<std::string>();
    const std::string normalizedContent = normalizePatchForGitApply(diffContent);
    const bool backup = args.value("backup", true);
    const bool dryRun = args.value("dry_run", false);

    auto fileDiffs = parseUnifiedDiff(normalizedContent);
    if (fileDiffs.empty()) {
        result["error"] = "diff_content 无效：未解析到任何文件补丁（diff --git / --- / +++ / @@）。";
        return result;
    }

    std::vector<std::string> affected;
    affected.reserve(fileDiffs.size());
    for (const auto& fd : fileDiffs) {
        std::string p = fd.isDeleted ? fd.oldFile : fd.newFile;
        if (p.empty()) p = fd.oldFile;
        p = stripGitPrefix(p);
        if (!p.empty()) affected.push_back(p);
    }

    // 保存 patch 历史（支持多次 undo）：每次 apply_patch 生成一个独立 patch 文件，并维护栈
    fs::path patchDir = rootPath / ".photon" / "patches";
    fs::create_directories(patchDir);
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ts;
    {
        struct tm time_info;
#ifdef _WIN32
        localtime_s(&time_info, &in_time_t);
#else
        localtime_r(&in_time_t, &time_info);
#endif
        ts << std::put_time(&time_info, "%Y%m%d_%H%M%S");
    }
    std::string stamp = ts.str();

    fs::path patchPath = patchDir / ("patch_" + stamp + ".patch");
    {
        std::ofstream pf(patchPath);
        pf << normalizedContent;
    }
    {
        nlohmann::json meta;
        meta["timestamp"] = static_cast<long long>(std::time(nullptr));
        meta["affected_files"] = affected;
        meta["patch_path"] = patchPath.u8string();
        meta["has_git"] = hasGit;
        std::ofstream mf(patchDir / ("patch_" + stamp + ".json"));
        mf << meta.dump(2);
    }

    // 更新 patch_stack.json
    fs::path stackPath = patchDir / "patch_stack.json";
    nlohmann::json stack = nlohmann::json::array();
    try {
        if (fs::exists(stackPath) && fs::is_regular_file(stackPath)) {
            std::ifstream sf(stackPath);
            if (sf.is_open()) {
                sf >> stack;
            }
        }
    } catch (...) {
        stack = nlohmann::json::array();
    }
    if (!stack.is_array()) stack = nlohmann::json::array();
    stack.push_back({
        {"timestamp", static_cast<long long>(std::time(nullptr))},
        {"patch_path", patchPath.u8string()},
        {"affected_files", affected}
    });
    {
        std::ofstream sf(stackPath);
        sf << stack.dump(2);
    }

    // 兼容：始终写一份 last.patch 指向最新补丁（供 patch/undo 旧逻辑和快速预览）
    {
        std::ofstream lf(patchDir / "last.patch");
        lf << normalizedContent;
    }
    {
        nlohmann::json lastMeta;
        lastMeta["timestamp"] = static_cast<long long>(std::time(nullptr));
        lastMeta["affected_files"] = affected;
        lastMeta["patch_path"] = patchPath.u8string();
        std::ofstream mf(patchDir / "last_patch.json");
        mf << lastMeta.dump(2);
    }

    if (hasGit) {
        std::string out;
        std::string prefix = "cd \"" + rootPath.u8string() + "\" && ";

        if (dryRun) {
            int code = execCaptureCoreTools(prefix + "git apply --whitespace=fix --check \"" + patchPath.u8string() + "\" 2>&1", out);
            result["success"] = (code == 0);
            result["dry_run"] = true;
            result["affected_files"] = affected;
            if (code != 0) result["error"] = out.empty() ? "git apply --check 失败" : out;
            else result["message"] = "Dry-run OK（git apply --check）";
            return result;
        }

        if (backup) {
            std::string status;
            execCaptureCoreTools(prefix + "git status --porcelain 2>&1", status);
            if (!status.empty()) {
                std::string stashOut;
                execCaptureCoreTools(prefix + "git stash push -u -m \"photon-apply_patch-backup\" 2>&1", stashOut);
                result["git_backup"] = "stash";
            } else {
                result["git_backup"] = "none(clean)";
            }
        }

        out.clear();
        int code = execCaptureCoreTools(prefix + "git apply --whitespace=fix \"" + patchPath.u8string() + "\" 2>&1", out);
        if (code != 0) {
            // 补丁是「新增文件」但工作区已有同名文件时，git apply 会报 "already exists in working directory"。
            // 回退到内置引擎：按 diff 内容写入/覆盖文件，便于 LLM 重试或覆盖已存在文件。
            bool alreadyExists = (out.find("already exists in working directory") != std::string::npos);
            if (alreadyExists) {
                std::string detail;
                if (applyUnifiedDiff(normalizedContent, &detail)) {
                    result["success"] = true;
                    result["affected_files"] = affected;
                    result["message"] = "目标文件已存在，已通过内置引擎覆盖应用补丁。可使用 undo 撤销。";
                    result["git_fallback"] = "already_exists";
                    return result;
                }
            }
            result["error"] = out.empty() ? "git apply 失败" : out;
            return result;
        }

        result["success"] = true;
        result["affected_files"] = affected;
        result["message"] = "已通过 git apply 应用补丁。可使用 undo 撤销上一次补丁。";
        return result;
    }

    if (dryRun) {
        result["success"] = true;
        result["dry_run"] = true;
        result["affected_files"] = affected;
        result["message"] = "无 Git 时 dry_run 仅做基础解析（建议启用 Git 以获得严格 check）。";
        return result;
    }

    if (backup) {
        for (const auto& fd : fileDiffs) {
            if (fd.isNewFile) continue;
            std::string p = stripGitPrefix(fd.oldFile.empty() ? fd.newFile : fd.oldFile);
            if (p.empty()) continue;
            fs::path fp = fs::u8path(p);
            if (!fp.is_absolute()) fp = rootPath / fp;
            if (!fs::exists(fp)) continue;
            try { createLocalBackup(p); } catch (...) {}
        }
    }

    std::string detail;
    if (!applyUnifiedDiff(normalizedContent, &detail)) {
        result["error"] = detail.empty() ? "手动 diff 引擎应用失败（通常是上下文不匹配）。建议安装/启用 Git 后再试。" : ("apply_patch 应用失败: " + detail);
        return result;
    }

    result["success"] = true;
    result["affected_files"] = affected;
    result["message"] = "已通过手动 unified-diff 引擎应用补丁。可使用 undo 尝试撤销上一次补丁。";
    return result;
}

// ============================================================================
// RunCommandTool Implementation
// ============================================================================

RunCommandTool::RunCommandTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string RunCommandTool::getDescription() const {
    return "Execute a shell command in the project directory (build, test, lint, list, logs, etc.). "
           "For creating or editing project files, use apply_patch instead. "
           "Parameters: command (string), timeout (int, optional, default 30 seconds).";
}

nlohmann::json RunCommandTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Command to execute"}
            }},
            {"timeout", {
                {"type", "integer"},
                {"description", "Timeout in seconds (default 30)"}
            }}
        }},
        {"required", {"command"}}
    };
}

nlohmann::json RunCommandTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("command")) {
        result["error"] = "Missing required parameter: command";
        return result;
    }
    
    std::string command = args["command"].get<std::string>();
    
    // 切换到项目目录并执行命令
    std::string fullCommand = "cd \"" + rootPath.u8string() + "\" && " + command + " 2>&1";
    
    std::array<char, 128> buffer;
    std::string output;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(fullCommand.c_str(), "r"), pclose);
    if (!pipe) {
        result["error"] = "Failed to execute command";
        return result;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    
    int exitCode = pclose(pipe.release()) / 256;
    
    // 返回结果
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = "Command: " + command + "\n" +
                          "Exit Code: " + std::to_string(exitCode) + "\n\n" +
                          "Output:\n" + output;
    
    result["content"] = nlohmann::json::array({contentItem});
    result["exit_code"] = exitCode;
    return result;
}

// ============================================================================
// ListProjectFilesTool Implementation
// ============================================================================

namespace {
bool isCodeFileForList(const std::string& filePath) {
    static const std::vector<std::string> codeExtensions = {
        ".cpp", ".h", ".hpp", ".cc", ".cxx", ".c", ".py", ".js", ".ts", ".jsx", ".tsx",
        ".java", ".go", ".rs", ".cs", ".rb", ".php", ".swift", ".kt", ".kts", ".ets"
    };
    std::string ext = fs::path(filePath).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return std::find(codeExtensions.begin(), codeExtensions.end(), ext) != codeExtensions.end();
}
// 紧凑格式：C:Class F:fn M:method，便于大模型理解且省 token
std::string formatSymbolsCompact(const std::vector<Symbol>& symbols, int maxCount) {
    std::string out;
    char typeChar = 'S';
    int n = 0;
    for (const auto& s : symbols) {
        if (n >= maxCount) break;
        if (!s.type.empty()) {
            switch (s.type[0]) {
                case 'c': typeChar = 'C'; break;  // class
                case 'f': typeChar = 'F'; break;  // function
                case 'm': typeChar = 'M'; break;  // method
                case 's': typeChar = (s.type.size() > 1 && s.type[1] == 't') ? 'S' : 'S'; break;  // struct
                case 'e': typeChar = 'E'; break;  // enum
                case 'i': typeChar = 'I'; break;  // interface
                default: typeChar = 'S'; break;
            }
        }
        if (!out.empty()) out += ' ';
        out += typeChar;
        out += ':';
        out += s.name;
        ++n;
    }
    return out;
}
// 从 key "path:line:name" 取出 name（path 可能含 :）
static std::string symbolKeyToName(const std::string& key) {
    size_t last = key.rfind(':');
    if (last == std::string::npos) return key;
    return key.substr(last + 1);
}
// 为 dictionary 生成紧凑调用链：sym→c1,c2 ←caller1；仅前 maxSymbols 个符号，每符号最多 maxCallees/maxCallers 个
std::string formatCallChainCompact(SymbolManager* symbolMgr, const std::vector<Symbol>& symbols,
                                  int maxSymbols, int maxCallees, int maxCallers) {
    if (!symbolMgr || symbols.empty()) return {};
    std::string out;
    int symCount = 0;
    for (const auto& sym : symbols) {
        if (symCount >= maxSymbols) break;
        auto callees = symbolMgr->getCalleesForSymbol(sym);
        auto callerKeys = symbolMgr->getCallerKeysForSymbol(sym);
        if (callees.empty() && callerKeys.empty()) continue;
        if (!out.empty()) out += "; ";
        out += sym.name;
        int n = 0;
        for (const auto& c : callees) {
            if (n >= maxCallees) break;
            if (n == 0) out += "→";
            else out += ",";
            out += symbolKeyToName(c);
            ++n;
        }
        n = 0;
        for (const auto& k : callerKeys) {
            if (n >= maxCallers) break;
            if (n == 0) out += " ←";
            else out += ",";
            out += symbolKeyToName(k);
            ++n;
        }
        ++symCount;
    }
    return out;
}
}  // namespace

ListProjectFilesTool::ListProjectFilesTool(const std::string& rootPath, SymbolManager* symbolMgr, int maxSymbolsPerFile,
                                           std::shared_ptr<ScanIgnoreRules> ignoreRules)
    : rootPath(fs::u8path(rootPath)), symbolMgr(symbolMgr), maxSymbolsPerFile(maxSymbolsPerFile), ignoreRules(std::move(ignoreRules)) {}

std::string ListProjectFilesTool::getDescription() const {
    return "List files and directories in the project. "
           "Code files show symbols (C=class F=function M=method S=struct E=enum I=interface) and call chain (name→callees ←callers). "
           "Parameters: path (optional, default '.'), max_depth (optional, default 3), include_symbols (optional, default true).";
}

nlohmann::json ListProjectFilesTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Relative path to list (default '.')"}
            }},
            {"max_depth", {
                {"type", "integer"},
                {"description", "Maximum depth to recurse (default 3)"}
            }},
            {"include_symbols", {
                {"type", "boolean"},
                {"description", "Attach symbol hints for code files (default true). Set false to save tokens when only structure is needed."}
            }}
        }}
    };
}

void ListProjectFilesTool::collectCodeFilePaths(const fs::path& dir, std::vector<std::string>& outPaths, int maxDepth, int currentDepth) {
    if (currentDepth > maxDepth) return;
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (ignoreRules) {
                if (ignoreRules->shouldIgnore(entry.path())) continue;
            } else {
                std::string name = entry.path().filename().u8string();
                if (name[0] == '.' || name == "node_modules" || name == "build" || name == "dist") continue;
            }
            if (entry.is_regular_file()) {
                std::string relPath = fs::relative(entry.path(), rootPath).u8string();
                if (isCodeFileForList(relPath)) outPaths.push_back(relPath);
            } else if (entry.is_directory() && currentDepth < maxDepth) {
                collectCodeFilePaths(entry.path(), outPaths, maxDepth, currentDepth + 1);
            }
        }
    } catch (const std::exception& e) { (void)e; }
}

void ListProjectFilesTool::listDirectory(const fs::path& dir, nlohmann::json& result, int maxDepth, int currentDepth,
                                        const std::unordered_map<std::string, std::vector<Symbol>>* symbolBatch) {
    if (currentDepth > maxDepth) return;
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (ignoreRules) {
                if (ignoreRules->shouldIgnore(entry.path())) continue;
            } else {
                std::string name = entry.path().filename().u8string();
                if (name[0] == '.' || name == "node_modules" || name == "build" || name == "dist") continue;
            }

            std::string name = entry.path().filename().u8string();
            nlohmann::json item;
            item["name"] = name;
            std::string relPath = fs::relative(entry.path(), rootPath).u8string();
            item["path"] = relPath;
            item["type"] = entry.is_directory() ? "directory" : "file";

            if (entry.is_regular_file()) {
                item["size"] = entry.file_size();
                if (symbolBatch) {
                    auto it = symbolBatch->find(relPath);
                    if (it != symbolBatch->end() && !it->second.empty()) {
                        std::string compact = formatSymbolsCompact(it->second, maxSymbolsPerFile);
                        if (!compact.empty()) item["sym"] = compact;
                        if (symbolMgr) {
                            std::string chain = formatCallChainCompact(symbolMgr, it->second, 3, 3, 3);
                            if (!chain.empty()) item["chain"] = chain;
                        }
                    }
                }
            }

            if (entry.is_directory() && currentDepth < maxDepth) {
                nlohmann::json children = nlohmann::json::array();
                listDirectory(entry.path(), children, maxDepth, currentDepth + 1, symbolBatch);
                item["children"] = children;
            }
            result.push_back(item);
        }
    } catch (const std::exception& e) { (void)e; }
}

static fs::path getProjectTreeCachePath(const fs::path& rootPath) {
    return rootPath / ".photon" / "index" / "project_tree.json";
}

bool ListProjectFilesTool::loadProjectTreeCache(nlohmann::json& outTree, std::string& outText) const {
    try {
        fs::path cachePath = getProjectTreeCachePath(rootPath);
        if (!fs::exists(cachePath)) return false;
        std::ifstream f(cachePath);
        if (!f.is_open()) return false;
        nlohmann::json j;
        f >> j;
        if (!j.is_object() || j.value("version", 0) != 1 || !j.contains("tree") || !j.contains("text")) return false;
        outTree = j["tree"];
        outText = j["text"].get<std::string>();
        return outTree.is_array() && !outText.empty();
    } catch (...) {
        return false;
    }
}

void ListProjectFilesTool::saveProjectTreeCache(const nlohmann::json& tree, const std::string& text, int pathMaxDepth) const {
    try {
        fs::path indexDir = rootPath / ".photon" / "index";
        fs::create_directories(indexDir);
        fs::path cachePath = getProjectTreeCachePath(rootPath);
        std::ofstream f(cachePath);
        if (!f.is_open()) return;
        nlohmann::json j;
        j["version"] = 1;
        j["path"] = ".";
        j["max_depth"] = pathMaxDepth;
        j["tree"] = tree;
        j["text"] = text;
        f << j.dump();
    } catch (...) {}
}

void ListProjectFilesTool::buildAndSaveCache(const std::string& rootPathStr, SymbolManager* symbolMgr, int maxDepth, int maxSymbolsPerFile,
                                            std::shared_ptr<ScanIgnoreRules> ignoreRules) {
    ListProjectFilesTool tool(rootPathStr, symbolMgr, maxSymbolsPerFile, std::move(ignoreRules));
    nlohmann::json res = tool.execute({{"path", "."}, {"max_depth", maxDepth}, {"include_symbols", true}});
    if (res.contains("error") || !res.contains("tree") || !res.contains("content") || !res["content"].is_array() || res["content"].empty()) return;
    std::string text = res["content"][0].value("text", "");
    if (text.empty()) return;
    tool.saveProjectTreeCache(res["tree"], text, maxDepth);
}

nlohmann::json ListProjectFilesTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    std::string path = args.value("path", ".");
    int maxDepth = args.value("max_depth", 3);
    bool includeSymbols = args.value("include_symbols", true);
    
    // 根目录 + 默认深度 + 需要 symbol 时优先用初始化时生成的持久化缓存
    if (path == "." && maxDepth == 3 && includeSymbols) {
        nlohmann::json cachedTree;
        std::string cachedText;
        if (loadProjectTreeCache(cachedTree, cachedText)) {
            if (cachedText.find("Legend:") == std::string::npos)
                cachedText = "Legend: C=class F=function M=method S=struct E=enum I=interface. Chain: name→calls ←called_by.\n\n" + cachedText;
            result["tree"] = cachedTree;
            result["content"] = nlohmann::json::array({{{"type", "text"}, {"text", cachedText}}});
            return result;
        }
    }
    
    fs::path fullPath = rootPath / fs::u8path(path);
    
    if (!fs::exists(fullPath)) {
        result["error"] = "Path not found: " + path;
        return result;
    }
    
    if (!fs::is_directory(fullPath)) {
        result["error"] = "Not a directory: " + path;
        return result;
    }
    
    std::unordered_map<std::string, std::vector<Symbol>> symbolBatch;
    if (includeSymbols && symbolMgr) {
        std::vector<std::string> codePaths;
        collectCodeFilePaths(fullPath, codePaths, maxDepth, 0);
        symbolMgr->getFileSymbolsBatch(codePaths, symbolBatch);
    }
    nlohmann::json tree = nlohmann::json::array();
    listDirectory(fullPath, tree, maxDepth, 0, symbolBatch.empty() ? nullptr : &symbolBatch);
    
    std::ostringstream treeText;
    treeText << "Project Structure: " << path << "\n\n";
    if (includeSymbols) {
        treeText << "Legend: C=class F=function M=method S=struct E=enum I=interface. Chain: name→calls ←called_by.\n\n";
    }
    
    std::function<void(const nlohmann::json&, int)> printTree;
    printTree = [&](const nlohmann::json& items, int depth) {
        for (const auto& item : items) {
            std::string indent(depth * 2, ' ');
            treeText << indent << "- " << item["name"].get<std::string>();
            
            if (item["type"] == "file" && item.contains("size")) {
                treeText << " (" << item["size"].get<size_t>() << " bytes)";
            }
            if (item.contains("sym")) {
                treeText << " [" << item["sym"].get<std::string>() << "]";
            }
            if (item.contains("chain")) {
                treeText << " | " << item["chain"].get<std::string>();
            }
            
            treeText << "\n";
            
            if (item.contains("children")) {
                printTree(item["children"], depth + 1);
            }
        }
    };
    
    printTree(tree, 0);
    
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = treeText.str();
    
    result["content"] = nlohmann::json::array({contentItem});
    result["tree"] = tree;
    return result;
}

// ============================================================================
// GrepTool Implementation
// ============================================================================

GrepTool::GrepTool(const std::string& rootPath, std::shared_ptr<ScanIgnoreRules> ignoreRules)
    : rootPath(fs::u8path(rootPath)), ignoreRules(std::move(ignoreRules)) {}

std::string GrepTool::getDescription() const {
    return "Search project files by text or regex (grep). Returns file, line, and matching line content. "
           "Use when you do not know which file contains something; then use read_code_block with the returned path and line. "
           "Parameters: pattern (required), path (optional, default '.'), include (optional glob, e.g. '*.cpp'), max_results (optional, default 200).";
}

nlohmann::json GrepTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "Search pattern (literal or regex). Escape for shell if needed."}
            }},
            {"path", {
                {"type", "string"},
                {"description", "Directory to search under (default '.'). Relative to project root."}
            }},
            {"include", {
                {"type", "string"},
                {"description", "Glob to include files, e.g. '*.cpp', '*.h' (optional)."}
            }},
            {"max_results", {
                {"type", "integer"},
                {"description", "Maximum number of matches to return (default 200)."}
            }}
        }},
        {"required", {"pattern"}}
    };
}

static void parseGrepLine(const std::string& line, std::string& outPath, int& outLine, std::string& outContent) {
    outPath.clear();
    outLine = 0;
    outContent.clear();
    if (line.empty()) return;
    // Format "path:line:content" — path may contain ':' (e.g. C:\a\b). Split from right: last ':' -> content, second-to-last -> line.
    size_t lastColon = line.rfind(':');
    if (lastColon == std::string::npos) return;
    outContent = line.substr(lastColon + 1);
    std::string rest = line.substr(0, lastColon);
    size_t prevColon = rest.rfind(':');
    if (prevColon == std::string::npos) return;
    outPath = rest.substr(0, prevColon);
    std::string lineStr = rest.substr(prevColon + 1);
    try {
        outLine = std::stoi(lineStr);
    } catch (...) {
        return;
    }
    if (outLine < 0) outLine = 0;
}

nlohmann::json GrepTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    if (!args.contains("pattern") || !args["pattern"].is_string()) {
        result["error"] = "Missing required parameter: pattern";
        return result;
    }
    std::string pattern = args["pattern"].get<std::string>();
    if (pattern.empty()) {
        result["error"] = "pattern must be non-empty";
        return result;
    }
    std::string searchPath = args.value("path", ".");
    std::string include = args.value("include", "");
    int maxResults = args.value("max_results", 200);
    if (maxResults <= 0 || maxResults > 2000) maxResults = 200;

    auto shellEscape = [](const std::string& s) -> std::string {
        if (s.find(' ') == std::string::npos && s.find('"') == std::string::npos && s.find('$') == std::string::npos) return s;
        std::string r;
        r.reserve(s.size() + 2);
        r += '"';
        for (char c : s) {
            if (c == '"' || c == '\\') r += '\\';
            r += c;
        }
        r += '"';
        return r;
    };
    std::string patternEsc = shellEscape(pattern);
#ifdef _WIN32
    // /d 使 cd 可跨盘符切换，避免 CI 上当前在 D: 而 temp 在 C: 导致找不到文件
    std::string prefix = "cd /d \"" + rootPath.u8string() + "\" && ";
#else
    std::string prefix = "cd \"" + rootPath.u8string() + "\" && ";
#endif
    std::string cmd;
    std::string out;
    // Prefer ripgrep (rg), then grep -rn; on Windows fallback to findstr when grep is missing
    bool useRg = false;
#ifdef _WIN32
    { std::string d; if (execCaptureCoreTools("where rg 2>nul", d) == 0 && !d.empty()) useRg = true; }
#else
    { std::string d; if (execCaptureCoreTools("which rg 2>/dev/null || command -v rg 2>/dev/null", d) == 0 && !d.empty()) useRg = true; }
#endif
    if (useRg) {
        cmd = prefix + "rg -n --no-heading --color never ";
        if (!include.empty()) cmd += "-g " + shellEscape(include) + " ";
        cmd += " -- " + patternEsc + " " + searchPath + " 2>&1";
    } else {
#ifdef _WIN32
        // Windows: findstr (grep usually not in PATH). /s=subdirs /n=line numbers; output file:line:content.
        std::string findstrPattern = pattern;
        for (size_t i = 0; i < findstrPattern.size(); ++i) {
            if (findstrPattern[i] == '"') { findstrPattern.insert(i, "\\"); ++i; }
        }
        // Use * to match all files (cmd expands in current dir after cd /d)
        cmd = prefix + "findstr /s /n /c:\"" + findstrPattern + "\" * 2>&1";
#else
        cmd = prefix + "grep -rn ";
        if (!include.empty()) cmd += "--include=" + shellEscape(include) + " ";
        cmd += " -e " + patternEsc + " " + searchPath + " 2>&1";
#endif
    }
    int code = execCaptureCoreTools(cmd, out);
    if (code != 0 && code != 1) {
        result["error"] = out.empty() ? "grep failed" : out;
        return result;
    }
    // Parse "path:line:content" lines；与 list/扫描共用忽略规则，跳过应忽略的文件
    nlohmann::json matches = nlohmann::json::array();
    std::istringstream iss(out);
    std::string line;
    int count = 0;
    while (count < maxResults && std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::string p, c;
        int ln = 0;
        parseGrepLine(line, p, ln, c);
        if (!p.empty() && ln > 0) {
            if (ignoreRules) {
                fs::path fullPath = rootPath / fs::u8path(p);
                if (ignoreRules->shouldIgnore(fullPath)) continue;
            }
            matches.push_back({{"file", p}, {"line", ln}, {"content", c}});
            ++count;
        }
    }
    result["matches"] = matches;
    result["count"] = static_cast<int>(matches.size());
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    std::ostringstream text;
    text << "grep pattern: " << pattern << "\nmatches: " << result["count"].get<int>() << "\n";
    for (const auto& m : matches) {
        text << m["file"].get<std::string>() << ":" << m["line"].get<int>() << ":" << m["content"].get<std::string>() << "\n";
    }
    contentItem["text"] = text.str();
    result["content"] = nlohmann::json::array({contentItem});
    return result;
}

// ============================================================================
// AttemptTool Implementation
// ============================================================================

AttemptTool::AttemptTool(const std::string& rootPath) : rootPath(fs::u8path(rootPath)) {}

std::string AttemptTool::getDescription() const {
    return "Maintain current user attempt (intent + task state) so the model does not forget across turns. "
           "Stored in .photon/current_attempt.json. Use at start of turn to recall what we are doing; "
           "update when user gives new requirement or when a step is done; clear when task is complete. "
           "Parameters: action (required: 'get' | 'update' | 'clear'). For update: intent, status, read_scope, affected_files, step_done (optional).";
}

nlohmann::json AttemptTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"action", {
                {"type", "string"},
                {"description", "get = return current attempt; update = merge fields and save; clear = remove attempt for new task"}
            }},
            {"intent", {
                {"type", "string"},
                {"description", "User intent / requirement description (for update)"}
            }},
            {"status", {
                {"type", "string"},
                {"description", "in_progress | done | blocked (for update)"}
            }},
            {"read_scope", {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", "Planned files or path::symbol to read (for update)"}
            }},
            {"affected_files", {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", "Files already or planned to be modified (for update)"}
            }},
            {"step_done", {
                {"type", "string"},
                {"description", "Append a completed step description (for update)"}
            }}
        }},
        {"required", {"action"}}
    };
}

nlohmann::json AttemptTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    if (!args.contains("action") || !args["action"].is_string()) {
        result["error"] = "Missing required parameter: action (get | update | clear)";
        return result;
    }
    std::string action = args["action"].get<std::string>();
    fs::path attemptPath = rootPath / ".photon" / "current_attempt.json";

    if (action == "get") {
        if (!fs::exists(attemptPath) || !fs::is_regular_file(attemptPath)) {
            result["attempt"] = nlohmann::json::object();
            result["message"] = "No current attempt; use update with intent to start one.";
        } else {
            try {
                std::ifstream f(attemptPath);
                nlohmann::json cur;
                f >> cur;
                result["attempt"] = cur;
            } catch (const std::exception& e) {
                result["error"] = std::string("Failed to read attempt: ") + e.what();
                return result;
            }
        }
        nlohmann::json contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = result.dump(2);
        result["content"] = nlohmann::json::array({contentItem});
        return result;
    }

    if (action == "clear") {
        std::error_code ec;
        if (fs::exists(attemptPath)) fs::remove(attemptPath, ec);
        result["message"] = "Attempt cleared. Ready for new task.";
        nlohmann::json contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = result.dump(2);
        result["content"] = nlohmann::json::array({contentItem});
        return result;
    }

    if (action == "update") {
        nlohmann::json cur;
        if (fs::exists(attemptPath) && fs::is_regular_file(attemptPath)) {
            try {
                std::ifstream f(attemptPath);
                f >> cur;
            } catch (...) {
                cur = nlohmann::json::object();
            }
        }
        if (!cur.is_object()) cur = nlohmann::json::object();

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::string ts = std::to_string(static_cast<long long>(t));
        if (!cur.contains("created_at") || cur["created_at"].is_null()) cur["created_at"] = ts;
        cur["updated_at"] = ts;

        if (args.contains("intent") && args["intent"].is_string()) cur["intent"] = args["intent"].get<std::string>();
        if (args.contains("status") && args["status"].is_string()) cur["status"] = args["status"].get<std::string>();
        if (args.contains("read_scope") && args["read_scope"].is_array()) cur["read_scope"] = args["read_scope"];
        if (args.contains("affected_files") && args["affected_files"].is_array()) cur["affected_files"] = args["affected_files"];
        if (args.contains("step_done") && args["step_done"].is_string()) {
            std::string step = args["step_done"].get<std::string>();
            if (!cur.contains("steps_completed")) cur["steps_completed"] = nlohmann::json::array();
            cur["steps_completed"].push_back(step);
        }

        if (!cur.contains("status")) cur["status"] = "in_progress";

        try {
            fs::create_directories(attemptPath.parent_path());
            std::ofstream of(attemptPath);
            of << cur.dump(2);
        } catch (const std::exception& e) {
            result["error"] = std::string("Failed to write attempt: ") + e.what();
            return result;
        }
        result["attempt"] = cur;
        result["message"] = "Attempt updated.";
        nlohmann::json contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = result.dump(2);
        result["content"] = nlohmann::json::array({contentItem});
        return result;
    }

    result["error"] = "action must be get, update, or clear";
    return result;
}

// ============================================================================
// SyntaxCheckTool Implementation
// ============================================================================

SyntaxCheckTool::SyntaxCheckTool(const std::string& rootPath, LspDiagnosticsFn getLspDiagnostics, HasLspForExtFn hasLspForExtension)
    : rootPath(fs::u8path(rootPath))
    , getLspDiagnostics(std::move(getLspDiagnostics))
    , hasLspForExtension(std::move(hasLspForExtension)) {}

std::string SyntaxCheckTool::getDescription() const {
    return "Syntax check: no need to pass files—automatically checks recent changes and new files (git). "
           "LSP first when available, else fallback (build/linter); neither then skip that language. "
           "Optional: max_output_lines, errors_only, build_dir.";
}

nlohmann::json SyntaxCheckTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"max_output_lines", {
                {"type", "integer"},
                {"description", "Max lines to return (default 60). Lower value saves tokens."}
            }},
            {"errors_only", {
                {"type", "boolean"},
                {"description", "If true, only include lines that look like errors (e.g. contain 'error:'). Further reduces tokens."}
            }},
            {"build_dir", {
                {"type", "string"},
                {"description", "C/C++ build directory for cmake --build (default 'build')."}
            }}
        }}
    };
}

static bool lineLooksLikeError(const std::string& line) {
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("error:") != std::string::npos ||
           lower.find("fatal error") != std::string::npos ||
           lower.find("error ") != std::string::npos;
}

// Syntax check language by extension. Supported:
//   cpp: .cpp .cc .cxx .hpp .h  -> cmake --build (filter by path) + optional LSP
//   c:   .c                     -> same as cpp
//   py:  .py                    -> python3 -m py_compile + optional LSP
//   ts:  .ts .tsx               -> npx tsc --noEmit (filter by path) + optional LSP
//   arkts: .ets                 -> ets2panda if in PATH + optional LSP
static std::string syntaxCheckLang(const std::string& path) {
    std::string ext;
    size_t dot = path.rfind('.');
    if (dot != std::string::npos && dot + 1 < path.size())
        ext = path.substr(dot);
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".h") return "cpp";
    if (ext == ".c") return "c";
    if (ext == ".py") return "py";
    if (ext == ".ets") return "arkts";
    if (ext == ".ts" || ext == ".tsx") return "ts";
    return "";
}

static void splitLines(const std::string& out, std::vector<std::string>& lines) {
    lines.clear();
    std::string line;
    for (char c : out) {
        if (c == '\n' || c == '\r') {
            if (!line.empty()) { lines.push_back(line); line.clear(); }
        } else
            line += c;
    }
    if (!line.empty()) lines.push_back(line);
}

// Keep only lines that contain any of the paths (e.g. "src/foo.cpp" or "foo.cpp").
static void filterLinesByPaths(std::vector<std::string>& lines, const std::set<std::string>& paths) {
    if (paths.empty()) return;
    std::vector<std::string> out;
    for (const auto& l : lines) {
        for (const auto& p : paths) {
            if (l.find(p) != std::string::npos) { out.push_back(l); break; }
        }
    }
    lines = std::move(out);
}

// Get modified/added files from git (relative paths). Returns empty if not git or error.
static std::vector<std::string> getGitModifiedFiles(const std::string& prefix) {
    std::string out;
    std::set<std::string> paths;
    execCaptureCoreTools(prefix + "git diff --name-only HEAD 2>/dev/null", out);
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) paths.insert(line);
    }
    out.clear();
    execCaptureCoreTools(prefix + "git diff --name-only --cached 2>/dev/null", out);
    iss.str(out);
    iss.clear();
    while (std::getline(iss, line)) {
        if (!line.empty()) paths.insert(line);
    }
    return std::vector<std::string>(paths.begin(), paths.end());
}

nlohmann::json SyntaxCheckTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    int maxLines = 60;
    if (args.contains("max_output_lines") && args["max_output_lines"].is_number_integer()) {
        int v = args["max_output_lines"].get<int>();
        if (v > 0 && v <= 500) maxLines = v;
    }
    bool errorsOnly = false;
    if (args.contains("errors_only") && args["errors_only"].is_boolean())
        errorsOnly = args["errors_only"].get<bool>();
    std::string buildDir = "build";
    if (args.contains("build_dir") && args["build_dir"].is_string() && !args["build_dir"].get<std::string>().empty())
        buildDir = args["build_dir"].get<std::string>();

    std::string prefix = "cd \"" + rootPath.u8string() + "\" && ";
    std::vector<std::string> modifiedFiles = getGitModifiedFiles(prefix);
    {
        std::vector<std::string> supported;
        for (const auto& p : modifiedFiles) {
            if (!syntaxCheckLang(p).empty()) supported.push_back(p);
        }
        modifiedFiles = std::move(supported);
    }

    std::vector<std::string> allLines;
    int globalExit = 0;
    std::set<std::string> cppPaths, tsPaths;

    auto useLspForExt = [this](const std::string& path) -> bool {
        if (!hasLspForExtension || !getLspDiagnostics) return false;
        size_t dot = path.rfind('.');
        if (dot == std::string::npos || dot + 1 >= path.size()) return false;
        std::string ext = path.substr(dot);
        return hasLspForExtension(ext);
    };
    auto runLspForFile = [this, &allLines, &globalExit](const std::string& p) {
        if (!getLspDiagnostics) return;
        std::string out = getLspDiagnostics(p);
        if (out.empty()) return;
        std::vector<std::string> lines;
        splitLines(out, lines);
        for (const auto& l : lines) {
            allLines.push_back(l);
            if (lineLooksLikeError(l)) globalExit = 1;
        }
    };

    if (!modifiedFiles.empty()) {
        for (const auto& p : modifiedFiles) {
            std::string lang = syntaxCheckLang(p);
            if (lang == "cpp" || lang == "c") cppPaths.insert(p);
            else if (lang == "ts") tsPaths.insert(p);
        }
    }

    // C/C++: LSP 优先，否则 cmake --build build_dir
    if (!cppPaths.empty()) {
        bool useLspCxx = (useLspForExt(".cpp") || useLspForExt(".c"));
        if (useLspCxx && getLspDiagnostics) {
            for (const auto& p : modifiedFiles)
                if (syntaxCheckLang(p) == "cpp" || syntaxCheckLang(p) == "c") runLspForFile(p);
        } else {
            std::string out;
            int code = execCaptureCoreTools(prefix + "cmake --build \"" + buildDir + "\" 2>&1", out);
            if (code != 0) globalExit = code;
            std::vector<std::string> lines;
            splitLines(out, lines);
            filterLinesByPaths(lines, cppPaths);
            for (const auto& l : lines) allLines.push_back(l);
        }
    }

    // Python: LSP 优先，否则 py_compile
    for (const auto& p : modifiedFiles) {
        if (syntaxCheckLang(p) != "py") continue;
        if (useLspForExt(p)) { runLspForFile(p); continue; }
        std::string out;
        std::string escaped = p;
        if (escaped.find('"') != std::string::npos) {
            std::string e; e.reserve(escaped.size() + 2);
            e += '"'; for (char c : escaped) { if (c == '"' || c == '\\') e += '\\'; e += c; }
            e += '"'; escaped = e;
        } else if (escaped.find(' ') != std::string::npos) escaped = "\"" + escaped + "\"";
        int code = execCaptureCoreTools(prefix + "python3 -m py_compile " + escaped + " 2>&1", out);
        if (code != 0) globalExit = code;
        std::vector<std::string> lines;
        splitLines(out, lines);
        for (const auto& l : lines) allLines.push_back(l);
    }

    // TypeScript: LSP 优先，否则 tsc --noEmit
    if (!tsPaths.empty()) {
        bool useLspTs = useLspForExt(".ts");
        if (useLspTs && getLspDiagnostics) {
            for (const auto& p : modifiedFiles)
                if (syntaxCheckLang(p) == "ts") runLspForFile(p);
        } else {
            std::string out;
            int code = execCaptureCoreTools(prefix + "npx tsc --noEmit 2>&1", out);
            if (code != 0) globalExit = code;
            std::vector<std::string> lines;
            splitLines(out, lines);
            filterLinesByPaths(lines, tsPaths);
            for (const auto& l : lines) allLines.push_back(l);
        }
    }

    // ArkTS: LSP 优先，否则 ets2panda，都没有则跳过
    std::string whichOut;
#ifdef _WIN32
    int hasEts = execCaptureCoreTools(prefix + "where ets2panda 2>nul", whichOut);
#else
    int hasEts = execCaptureCoreTools(prefix + "which ets2panda 2>/dev/null || command -v ets2panda 2>/dev/null", whichOut);
#endif
    bool hasEts2panda = (hasEts == 0 && !whichOut.empty());
    for (const auto& p : modifiedFiles) {
        if (syntaxCheckLang(p) != "arkts") continue;
        if (useLspForExt(p)) { runLspForFile(p); continue; }
        if (!hasEts2panda) continue;
        std::string out;
        std::string escaped = p;
        if (escaped.find('"') != std::string::npos) {
            std::string e; e.reserve(escaped.size() + 2);
            e += '"'; for (char c : escaped) { if (c == '"' || c == '\\') e += '\\'; e += c; }
            e += '"'; escaped = e;
        } else if (escaped.find(' ') != std::string::npos) escaped = "\"" + escaped + "\"";
        int code = execCaptureCoreTools(prefix + "ets2panda " + escaped + " 2>&1", out);
        if (code != 0) globalExit = code;
        if (!out.empty()) {
            std::vector<std::string> lines;
            splitLines(out, lines);
            for (const auto& l : lines) allLines.push_back(l);
        }
    }

    if (modifiedFiles.empty()) {
        allLines.push_back("(no modified files; nothing to check)");
    }

    if (errorsOnly) {
        std::vector<std::string> filtered;
        for (const auto& l : allLines) {
            if (lineLooksLikeError(l)) filtered.push_back(l);
        }
        allLines = std::move(filtered);
    }
    std::string truncNote;
    if (allLines.size() > static_cast<size_t>(maxLines)) {
        allLines.resize(maxLines);
        truncNote = "(output truncated to " + std::to_string(maxLines) + " lines)\n";
    }
    std::string text = truncNote;
    for (const auto& l : allLines) text += l + "\n";

    result["success"] = (globalExit == 0);
    result["exit_code"] = globalExit;
    result["content"] = nlohmann::json::array({
        nlohmann::json::object({{"type", "text"}, {"text", "Exit: " + std::to_string(globalExit) + "\n\n" + text}})
    });
    if (!modifiedFiles.empty())
        result["modified_files_checked"] = modifiedFiles.size();
    return result;
}

// ============================================================================
// SkillActivateTool Implementation
// ============================================================================

#include "utils/SkillManager.h"

SkillActivateTool::SkillActivateTool(SkillManager* skillManager)
    : skillMgr(skillManager) {}

std::string SkillActivateTool::getDescription() const {
    return "Activate a specialized skill to access its capabilities. "
           "Once activated, the skill's tools and constraints will be injected "
           "into your context. Parameters: skill_name (string).";
}

nlohmann::json SkillActivateTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"skill_name", {
                {"type", "string"},
                {"description", "Name of the skill to activate"}
            }}
        }},
        {"required", {"skill_name"}}
    };
}

nlohmann::json SkillActivateTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("skill_name")) {
        result["error"] = "Missing required parameter: skill_name";
        return result;
    }
    
    std::string skillName = args["skill_name"].get<std::string>();
    
    if (!skillMgr) {
        result["error"] = "SkillManager not available";
        return result;
    }
    
    if (skillMgr->activate(skillName)) {
        result["success"] = true;
        result["message"] = "Skill activated: " + skillName;
        result["active_skills"] = skillMgr->getActiveSkills();
        
        // 返回 Skill 的 Prompt 片段
        result["skill_prompt"] = skillMgr->getActiveSkillsPrompt();
    } else {
        result["error"] = "Failed to activate skill: " + skillName;
        result["hint"] = "Check if skill exists in allowlist";
    }
    
    return result;
}

// ============================================================================
// SkillDeactivateTool Implementation
// ============================================================================

SkillDeactivateTool::SkillDeactivateTool(SkillManager* skillManager)
    : skillMgr(skillManager) {}

std::string SkillDeactivateTool::getDescription() const {
    return "Deactivate a previously activated skill to free up context space. "
           "Parameters: skill_name (string).";
}

nlohmann::json SkillDeactivateTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"skill_name", {
                {"type", "string"},
                {"description", "Name of the skill to deactivate"}
            }}
        }},
        {"required", {"skill_name"}}
    };
}

nlohmann::json SkillDeactivateTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("skill_name")) {
        result["error"] = "Missing required parameter: skill_name";
        return result;
    }
    
    std::string skillName = args["skill_name"].get<std::string>();
    
    if (!skillMgr) {
        result["error"] = "SkillManager not available";
        return result;
    }
    
    skillMgr->deactivate(skillName);
    
    result["success"] = true;
    result["message"] = "Skill deactivated: " + skillName;
    result["active_skills"] = skillMgr->getActiveSkills();
    
    return result;
}

// DiffBackupTool 已移除：统一能力由 ApplyPatchTool（unified diff）提供
