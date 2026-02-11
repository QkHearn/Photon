#include "CoreTools.h"
#include "analysis/SymbolManager.h"
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
           "(2) symbol_name specified → returns that symbol's code; "
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
    return readLineRange(filePath, targetSymbol->line, targetSymbol->endLine);
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
    std::string desc = "Modify project files by applying a unified diff (recommended for all file edits: reversible, trackable). "
                      "Provide diff_content: each line added with '+' prefix, removed with '-' prefix, unchanged with space. "
                      "Include at least one hunk header like \"@@ -1,3 +1,4 @@\". "
                      "Multi-file, backup and rollback supported. ";
    
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
                {"description", "Unified diff string. Each line must start with '+', '-' or space, and the diff must contain at least one '@@' hunk header."}
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
                {"description", "Preview changes without applying (default: false)"}
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

bool ApplyPatchTool::applyFileChanges(const FileDiff& fileDiff) {
    std::string rel = fileDiff.isDeleted ? fileDiff.oldFile : fileDiff.newFile;
    if (rel.empty()) rel = fileDiff.oldFile;
    rel = stripGitPrefix(rel);
    if (rel.empty()) return false;

    fs::path full = fs::u8path(rel);
    if (!full.is_absolute()) full = rootPath / full;

    if (fileDiff.isDeleted) {
        if (!fs::exists(full)) return true;
        std::error_code ec;
        fs::remove(full, ec);
        return !ec;
    }

    std::vector<std::string> original;
    if (fs::exists(full)) {
        std::ifstream in(full);
        if (!in.is_open()) return false;
        std::string l;
        while (std::getline(in, l)) original.push_back(l);
    }

    std::vector<std::string> out;
    size_t oldIdx = 0;

    for (const auto& h : fileDiff.hunks) {
        size_t targetOld0 = (h.oldStart <= 0) ? 0 : static_cast<size_t>(h.oldStart - 1);
        if (targetOld0 > original.size()) return false;
        while (oldIdx < targetOld0) out.push_back(original[oldIdx++]);

        for (const auto& hl : h.lines) {
            if (hl.empty()) continue;
            char prefix = hl[0];
            std::string content = hl.substr(1);
            if (prefix == ' ') {
                if (oldIdx >= original.size() || original[oldIdx] != content) return false;
                out.push_back(content);
                oldIdx++;
            } else if (prefix == '-') {
                if (oldIdx >= original.size() || original[oldIdx] != content) return false;
                oldIdx++;
            } else if (prefix == '+') {
                out.push_back(content);
            }
        }
    }

    while (oldIdx < original.size()) out.push_back(original[oldIdx++]);

    fs::create_directories(full.parent_path());
    std::ofstream of(full);
    if (!of.is_open()) return false;
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

bool ApplyPatchTool::applyUnifiedDiff(const std::string& diffContent) {
    auto diffs = parseUnifiedDiff(diffContent);
    if (diffs.empty()) return false;
    for (const auto& fd : diffs) {
        if (!applyFileChanges(fd)) return false;
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
    const bool backup = args.value("backup", true);
    const bool dryRun = args.value("dry_run", false);

    auto fileDiffs = parseUnifiedDiff(diffContent);
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
        pf << diffContent;
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
        lf << diffContent;
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
            int code = execCaptureCoreTools(prefix + "git apply --check \"" + patchPath.u8string() + "\" 2>&1", out);
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
        int code = execCaptureCoreTools(prefix + "git apply \"" + patchPath.u8string() + "\" 2>&1", out);
        if (code != 0) {
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

    if (!applyUnifiedDiff(diffContent)) {
        result["error"] = "手动 diff 引擎应用失败（通常是上下文不匹配）。建议安装/启用 Git 后再试。";
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
    return "Execute a shell command in the project directory. "
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

ListProjectFilesTool::ListProjectFilesTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string ListProjectFilesTool::getDescription() const {
    return "List files and directories in the project. "
           "Parameters: path (string, optional, default '.'), max_depth (int, optional, default 3).";
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
            }}
        }}
    };
}

void ListProjectFilesTool::listDirectory(const fs::path& dir, nlohmann::json& result, int maxDepth, int currentDepth) {
    if (currentDepth > maxDepth) return;
    
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::string name = entry.path().filename().u8string();
            
            // 跳过隐藏文件和常见忽略目录
            if (name[0] == '.' || name == "node_modules" || name == "build" || name == "dist") {
                continue;
            }
            
            nlohmann::json item;
            item["name"] = name;
            item["path"] = fs::relative(entry.path(), rootPath).u8string();
            item["type"] = entry.is_directory() ? "directory" : "file";
            
            if (entry.is_regular_file()) {
                item["size"] = entry.file_size();
            }
            
            if (entry.is_directory() && currentDepth < maxDepth) {
                nlohmann::json children = nlohmann::json::array();
                listDirectory(entry.path(), children, maxDepth, currentDepth + 1);
                item["children"] = children;
            }
            
            result.push_back(item);
        }
    } catch (const std::exception& e) {
        // 忽略权限错误等
    }
}

nlohmann::json ListProjectFilesTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    std::string path = args.value("path", ".");
    int maxDepth = args.value("max_depth", 3);
    
    fs::path fullPath = rootPath / fs::u8path(path);
    
    if (!fs::exists(fullPath)) {
        result["error"] = "Path not found: " + path;
        return result;
    }
    
    if (!fs::is_directory(fullPath)) {
        result["error"] = "Not a directory: " + path;
        return result;
    }
    
    nlohmann::json tree = nlohmann::json::array();
    listDirectory(fullPath, tree, maxDepth, 0);
    
    // 构建可读的树形结构文本
    std::ostringstream treeText;
    treeText << "Project Structure: " << path << "\n\n";
    
    std::function<void(const nlohmann::json&, int)> printTree;
    printTree = [&](const nlohmann::json& items, int depth) {
        for (const auto& item : items) {
            std::string indent(depth * 2, ' ');
            treeText << indent << "- " << item["name"].get<std::string>();
            
            if (item["type"] == "file" && item.contains("size")) {
                treeText << " (" << item["size"].get<size_t>() << " bytes)";
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
