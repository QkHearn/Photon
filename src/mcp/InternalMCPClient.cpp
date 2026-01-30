#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "mcp/InternalMCPClient.h"
#include "utils/LogicMapper.h"
#include <algorithm>
#include <regex>
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include <future>
#include <thread>
#include <mutex>
#include <unordered_set>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

InternalMCPClient::InternalMCPClient(const std::string& rootPathStr) {
    this->rootPath = fs::u8path(rootPathStr);
    
    // Determine global data path (same directory as the executable)
    try {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        this->globalDataPath = fs::path(path).parent_path() / ".photon";
#elif defined(__APPLE__)
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            this->globalDataPath = fs::canonical(path).parent_path() / ".photon";
        } else {
            this->globalDataPath = this->rootPath / ".photon";
        }
#else
        this->globalDataPath = fs::canonical("/proc/self/exe").parent_path() / ".photon";
#endif
    } catch (...) {
        this->globalDataPath = this->rootPath / ".photon"; // Fallback
    }

    isGitRepo = checkGitRepo();
    registerTools();
    
    // Move non-critical background tasks to a separate thread to speed up startup
    std::thread([this]() {
        loadTasksFromDisk(); // Reload tasks from previous sessions
        syncKnowledgeIndex(); // Sync manually added MD files
    }).detach();
}

void InternalMCPClient::syncKnowledgeIndex() {
    fs::path memoryPath = globalDataPath / "memory.json";
    nlohmann::json memory = nlohmann::json::object();
    
    if (fs::exists(memoryPath)) {
        std::ifstream f(memoryPath);
        try { f >> memory; } catch (...) {}
    }

    if (!memory.contains("knowledge_files")) {
        memory["knowledge_files"] = nlohmann::json::array();
    }

    bool changed = false;
    std::vector<std::string> currentFiles;
    
    try {
        if (fs::exists(globalDataPath)) {
            for (const auto& entry : fs::directory_iterator(globalDataPath)) {
                if (entry.path().extension() == ".md") {
                    currentFiles.push_back(entry.path().filename().u8string());
                }
            }
        }
    } catch (...) {}

    // Check for new files or removed files
    nlohmann::json& existingFiles = memory["knowledge_files"];
    bool needsUpdate = false;
    if (existingFiles.size() != currentFiles.size()) {
        needsUpdate = true;
    } else {
        for (const auto& f : currentFiles) {
            bool found = false;
            for (const auto& ex : existingFiles) {
                if (ex.get<std::string>() == f) { found = true; break; }
            }
            if (!found) { needsUpdate = true; break; }
        }
    }

    if (needsUpdate) {
        existingFiles = nlohmann::json::array();
        for (const auto& f : currentFiles) {
            existingFiles.push_back(f);
        }
        changed = true;
    }

    if (changed) {
        std::ofstream f(memoryPath);
        if (f.is_open()) {
            f << memory.dump(4);
        }
    }
}

void InternalMCPClient::registerTools() {
    toolHandlers["read"] = [this](const nlohmann::json& a) { return toolFileRead(a); };
    toolHandlers["search"] = [this](const nlohmann::json& a) { return toolSearch(a); };
    toolHandlers["write"] = [this](const nlohmann::json& a) { return toolFileWrite(a); };
    toolHandlers["history"] = [this](const nlohmann::json& a) { return toolHistory(a); };
    toolHandlers["explore"] = [this](const nlohmann::json& a) { return toolFileExplore(a); };
    toolHandlers["diagnostics_check"] = [this](const nlohmann::json& a) { return diagnosticsCheck(a); };
    toolHandlers["python_sandbox"] = [this](const nlohmann::json& a) { return pythonSandbox(a); };
    toolHandlers["pip_install"] = [this](const nlohmann::json& a) { return pipInstall(a); };
    toolHandlers["reason"] = [this](const nlohmann::json& a) { return toolReason(a); };
    toolHandlers["arch_visualize"] = [this](const nlohmann::json& a) { return archVisualize(a); };
    toolHandlers["bash_execute"] = [this](const nlohmann::json& a) { return bashExecute(a); };
    toolHandlers["git_operations"] = [this](const nlohmann::json& a) { return gitOperations(a); };
    toolHandlers["web_fetch"] = [this](const nlohmann::json& a) { return webFetch(a); };
    toolHandlers["web_search"] = [this](const nlohmann::json& a) { return webSearch(a); };
    toolHandlers["harmony_search"] = [this](const nlohmann::json& a) { return harmonySearch(a); };
    toolHandlers["file_undo"] = [this](const nlohmann::json& a) { return fileUndo(a); };
    toolHandlers["memory"] = [this](const nlohmann::json& a) { return toolMemory(a); };
    toolHandlers["project_overview"] = [this](const nlohmann::json& a) { return projectOverview(a); };
    toolHandlers["plan"] = [this](const nlohmann::json& a) { return toolPlan(a); };
    toolHandlers["lsp_definition"] = [this](const nlohmann::json& a) { return lspDefinition(a); };
    toolHandlers["lsp_references"] = [this](const nlohmann::json& a) { return lspReferences(a); };
    toolHandlers["resolve_relative_date"] = [this](const nlohmann::json& a) { return resolveRelativeDate(a); };
    toolHandlers["skill_read"] = [this](const nlohmann::json& a) { return skillRead(a); };
    toolHandlers["schedule"] = [this](const nlohmann::json& a) { return osScheduler(a); };
    toolHandlers["tasks"] = [this](const nlohmann::json& a) { return listTasks(a); };
    toolHandlers["cancel_task"] = [this](const nlohmann::json& a) { return cancelTask(a); };
    toolHandlers["read_task_log"] = [this](const nlohmann::json& a) { return readTaskLog(a); };
}

InternalMCPClient::~InternalMCPClient() {
    // Kill all tracked background tasks on exit
    for (const auto& task : tasks) {
        killTaskProcess(task.pid);
    }
}

std::string InternalMCPClient::generateTaskId() {
    static int counter = 0;
    return "task_" + std::to_string(++counter);
}

void InternalMCPClient::killTaskProcess(int pid) {
    if (pid <= 0) return;
#ifdef _WIN32
    std::string cmd = "taskkill /F /PID " + std::to_string(pid) + " >nul 2>&1";
#else
    // Use PID directly instead of process group to avoid "No such process" errors
    // and suppress stderr to keep the console clean.
    std::string cmd = "kill -9 " + std::to_string(pid) + " >/dev/null 2>&1"; 
#endif
    auto ignored = std::system(cmd.c_str());
    (void)ignored;
}

bool InternalMCPClient::checkGitRepo() {
    fs::path gitDir = rootPath / ".git";
    return fs::exists(gitDir) && fs::is_directory(gitDir);
}

bool InternalMCPClient::isBinary(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    char buffer[1024];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes = file.gcount();
    for (std::streamsize i = 0; i < bytes; ++i) {
        if (buffer[i] == '\0') return true;
    }
    return false;
}

bool InternalMCPClient::isDeclarativeFile(const std::string& relPath) const {
    std::string ext = fs::path(relPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    static const std::unordered_set<std::string> declarativeExts = {
        ".h", ".hpp", ".hh", ".hxx", ".idl", ".proto", ".api", ".yaml", ".yml", ".json"
    };
    return declarativeExts.count(ext) > 0;
}

int InternalMCPClient::countFileLines(const fs::path& fullPath) const {
    std::ifstream file(fullPath);
    if (!file.is_open()) return -1;
    int lines = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lines;
    }
    return lines;
}

// Helper to extract tag name from a tag string (e.g., "div class='foo'" -> "div")
std::string getTagName(const std::string& tag) {
    std::string name = tag;
    size_t space = name.find_first_of(" \t\r\n/");
    if (space != std::string::npos) name = name.substr(0, space);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (!name.empty() && name[0] == '/') name = name.substr(1);
    return name;
}

std::string InternalMCPClient::cleanHtml(const std::string& html) {
    std::string result;
    result.reserve(html.length());
    
    bool inTag = false;
    bool inScriptOrStyle = false;
    std::string currentTag;

    for (size_t i = 0; i < html.length(); ++i) {
        char c = html[i];

        if (c == '<') {
            inTag = true;
            currentTag.clear();
            continue;
        }
        
        if (inTag) {
            if (c == '>') {
                inTag = false;
                std::string tagName = getTagName(currentTag);
                bool isClosing = (!currentTag.empty() && currentTag[0] == '/');
                
                if (tagName == "script" || tagName == "style") {
                    inScriptOrStyle = !isClosing;
                } else if (tagName == "p" || tagName == "div" || tagName == "br" || tagName == "li" || tagName == "tr" || tagName == "h1" || tagName == "h2" || tagName == "h3") {
                    if (!result.empty() && result.back() != '\n') result += '\n';
                }
                continue;
            }
            currentTag += c;
            continue;
        }

        if (!inScriptOrStyle) {
            result += c;
        }
    }

    // Fast entity replacement
    auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(result, "&nbsp;", " ");
    replaceAll(result, "&lt;", "<");
    replaceAll(result, "&gt;", ">");
    replaceAll(result, "&amp;", "&");
    replaceAll(result, "&quot;", "\"");
    replaceAll(result, "&apos;", "'");

    // Remove excessive newlines
    static const std::regex multiple_newlines(R"raw(\n{3,})raw");
    result = std::regex_replace(result, multiple_newlines, "\n\n");
    
    // Trim
    size_t first = result.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) return "";
    size_t last = result.find_last_not_of(" \n\r\t");
    return result.substr(first, (last - first + 1));
}

std::string InternalMCPClient::htmlToMarkdown(const std::string& html) {
    std::string result;
    bool inTag = false;
    bool inScriptOrStyle = false;
    std::string currentTag;
    std::string currentLink;
    bool inLink = false;

    for (size_t i = 0; i < html.length(); ++i) {
        char c = html[i];

        if (c == '<') {
            inTag = true;
            currentTag.clear();
            continue;
        }
        
        if (inTag) {
            if (c == '>') {
                inTag = false;
                std::string tagName = getTagName(currentTag);
                bool isClosing = (!currentTag.empty() && currentTag[0] == '/');
                
                if (tagName == "script" || tagName == "style") {
                    inScriptOrStyle = !isClosing;
                } else if (tagName == "h1" || tagName == "h2") {
                    result += isClosing ? "\n" : "\n\n# ";
                } else if (tagName == "h3" || tagName == "h4") {
                    result += isClosing ? "\n" : "\n\n## ";
                } else if (tagName == "p" || tagName == "div" || tagName == "tr") {
                    if (!isClosing && !result.empty() && result.back() != '\n') result += "\n";
                } else if (tagName == "br") {
                    result += "\n";
                } else if (tagName == "li") {
                    if (!isClosing) result += "\n* ";
                } else if (tagName == "a" && !isClosing) {
                    // Simple href extraction, protecting the URL
                    size_t hrefPos = currentTag.find("href=\"");
                    if (hrefPos == std::string::npos) hrefPos = currentTag.find("href='");
                    
                    if (hrefPos != std::string::npos) {
                        char quote = currentTag[hrefPos + 5];
                        size_t endQuote = currentTag.find(quote, hrefPos + 6);
                        if (endQuote != std::string::npos) {
                            currentLink = currentTag.substr(hrefPos + 6, endQuote - (hrefPos + 6));
                            if (currentLink.find("http") == 0 || currentLink.find("/") == 0) {
                                result += "[";
                                inLink = true;
                            }
                        }
                    }
                } else if (tagName == "a" && isClosing && inLink) {
                    result += "](" + currentLink + ")";
                    inLink = false;
                }
                continue;
            }
            currentTag += c;
            continue;
        }

        if (!inScriptOrStyle) {
            result += c;
        }
    }

    // Entities
    auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    replaceAll(result, "&nbsp;", " ");
    replaceAll(result, "&lt;", "<");
    replaceAll(result, "&gt;", ">");
    replaceAll(result, "&amp;", "&");
    replaceAll(result, "&quot;", "\"");
    
    return result;
}

// Minimal HTML tag stripping and entity decoding to keep search results readable
std::string minimalStrip(const std::string& input) {
    if (input.empty()) return "";
    // Remove tags but protect common structure
    std::string s = std::regex_replace(input, std::regex("<[^>]*>"), "");
    
    // Quick entity replacement
    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    replaceAll(s, "&nbsp;", " ");
    replaceAll(s, "&lt;", "<");
    replaceAll(s, "&gt;", ">");
    replaceAll(s, "&amp;", "&");
    replaceAll(s, "&quot;", "\"");
    replaceAll(s, "&apos;", "'");
    
    // Replace newlines/tabs with spaces to avoid breaking Markdown structure
    for (char& c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    
    // Trim
    size_t first = s.find_first_not_of(" ");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" ");
    return s.substr(first, (last - first + 1));
}

// Helper for URL encoding
std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char)c) << std::nouppercase;
    }

    return escaped.str();
}

// Helper to safely truncate and sanitize UTF-8 strings
std::string sanitizeUtf8(std::string str, size_t maxLen) {
    if (str.length() > maxLen) {
        str = str.substr(0, maxLen);
        // Backtrack to avoid cutting in the middle of a multi-byte character
        while (!str.empty() && (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80) {
            str.pop_back();
        }
        // If the last byte is the start of a multi-byte character, pop it too
        if (!str.empty() && (static_cast<unsigned char>(str.back()) & 0x80) != 0) {
            str.pop_back();
        }
        str += "... (truncated)";
    }

    // Replace invalid UTF-8 sequences to prevent JSON errors
    std::string sanitized;
    sanitized.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = str[i];
        if (c < 0x80) {
            sanitized += c;
        } else if ((c & 0xE0) == 0xC0) { // 2-byte
            if (i + 1 < str.size() && (str[i+1] & 0xC0) == 0x80) {
                sanitized += c; sanitized += str[++i];
            } else sanitized += '?';
        } else if ((c & 0xF0) == 0xE0) { // 3-byte
            if (i + 2 < str.size() && (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80) {
                sanitized += c; sanitized += str[++i]; sanitized += str[++i];
            } else sanitized += '?';
        } else if ((c & 0xF8) == 0xF0) { // 4-byte
            if (i + 3 < str.size() && (str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80 && (str[i+3] & 0xC0) == 0x80) {
                sanitized += c; sanitized += str[++i]; sanitized += str[++i]; sanitized += str[++i];
            } else sanitized += '?';
        } else {
            sanitized += '?';
        }
    }
    return sanitized;
}

bool InternalMCPClient::shouldIgnore(const fs::path& path) {
    for (auto it = path.begin(); it != path.end(); ++it) {
        std::string part = it->string();
        if (part == ".git" || part == "node_modules" || part == "build" || 
            part == ".idea" || part == ".photon" || part.find("cmake-build-") == 0) {
            return true;
        }
    }
    return false;
}

// Helper for URL decoding
std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%') {
            if (i + 2 < value.length()) {
                int hex = std::stoi(value.substr(i + 1, 2), nullptr, 16);
                result += static_cast<char>(hex);
                i += 2;
            }
        } else if (value[i] == '+') {
            result += ' ';
        } else {
            result += value[i];
        }
    }
    return result;
}

nlohmann::json InternalMCPClient::listTools() {
    nlohmann::json tools = nlohmann::json::array();

    // Consolidated Read Tool
    tools.push_back({
        {"name", "read"},
        {"description", "Unified code read. DEFAULT to symbol or range, NOT full. Use mode symbol(name) or range(start/end) first; use full ONLY when file is small (<200 lines) or scope already narrowed. "
                        "Params: path + mode + reason. mode: symbol, range(start/end), full (last resort). "
                        "Using full on large files wastes tokens; prefer symbol/range."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "Relative path of the file"}}},
                {"reason", {{"type", "string"}, {"description", "Why this read is necessary"}}},
                {"priority", {{"type", "string"}, {"enum", {"low", "normal", "high"}}}},
                {"mode", {
                    {"type", "object"},
                    {"properties", {
                        {"type", {{"type", "string"}, {"enum", {"full", "range", "symbol"}}}},
                        {"start", {{"type", "integer"}}},
                        {"end", {{"type", "integer"}}},
                        {"name", {{"type", "string"}}}
                    }},
                    {"required", {"type"}}
                }},
                {"summary", {{"type", "string"}, {"description", "Optional short summary of the previous read"}}},
                {"start_line", {{"type", "integer"}, {"description", "Legacy range start (1-based)"}}},
                {"end_line", {{"type", "integer"}, {"description", "Legacy range end (1-based)"}}},
                {"query", {{"type", "string"}, {"description", "Legacy symbol name"}}},
                {"center_line", {{"type", "integer"}, {"description", "Optional center line for context window"}}},
                {"before", {{"type", "integer"}, {"description", "Lines before center (default: 20, max: 30)"}}},
                {"after", {{"type", "integer"}, {"description", "Lines after center (default: 20, max: 30)"}}}
            }},
            {"required", {"mode", "reason"}}
        }}
    });

    tools.push_back({
        {"name", "search"},
        {"description", "Unified code search entry. Provide a SearchRequest with type + query. "
                        "type is one of: symbol, text, file, semantic. "
                        "Runtime may infer type when omitted."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"type", {{"type", "string"}, {"enum", {"symbol", "text", "file", "semantic"}}}},
                {"query", {{"type", "string"}, {"description", "Search query"}}},
                {"path", {{"type", "string"}, {"description", "Optional path filter"}}},
                {"directory", {{"type", "string"}, {"description", "Optional directory filter"}}},
                {"max_results", {{"type", "integer"}, {"description", "Max results (default: 5)"}}}
            }},
            {"required", {"query"}}
        }}
    });

    tools.push_back({
        {"name", "plan"},
        {"description", "Unified plan entry. Provide query + optional options. Returns a structured plan to guide read/search/write."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "User intent or target"}}},
                {"path", {{"type", "string"}, {"description", "Optional path filter"}}},
                {"top_k", {{"type", "integer"}, {"description", "Top semantic chunks (default: 5)"}}},
                {"max_entries", {{"type", "integer"}, {"description", "Max entry candidates (default: 5)"}}},
                {"max_calls", {{"type", "integer"}, {"description", "Max call hints per entry (default: 10)"}}},
                {"include_semantic", {{"type", "boolean"}, {"description", "Include semantic fallback (default: false)"}}},
                {"options", {
                    {"type", "object"},
                    {"properties", {
                        {"budget", {{"type", "integer"}}},
                        {"max_steps", {{"type", "integer"}}}
                    }}
                }}
            }},
            {"required", {"query"}}
        }}
    });

    tools.push_back({
        {"name", "reason"},
        {"description", "Unified reasoning entry. Provide type + targets to run call graph, dependency, or logic checks."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"type", {{"type", "string"}, {"enum", {"call_graph", "dependency", "logic_check", "summary_inference"}}}},
                {"targets", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"target", {{"type", "string"}}},
                {"path", {{"type", "string"}}},
                {"max_depth", {{"type", "integer"}, {"description", "Max depth for call graph (default: 3)"}}}
            }},
            {"required", {"type"}}
        }}
    });

    tools.push_back({
        {"name", "history"},
        {"description", "Unified history entry. Append/query/remove operation history with filters."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"action", {{"type", "string"}, {"enum", {"append", "query", "remove"}}}},
                {"operation", {{"type", "object"}, {"description", "Operation to append"}}},
                {"filter", {
                    {"type", "object"},
                    {"properties", {
                        {"tool", {{"type", "string"}}},
                        {"path", {{"type", "string"}}},
                        {"contains", {{"type", "string"}}},
                        {"limit", {{"type", "integer"}}}
                    }}
                }}
            }},
            {"required", {"action"}}
        }}
    });

    tools.push_back({
        {"name", "memory"},
        {"description", "Unified memory entry. action: write/read/query/delete. Supports facts and knowledge."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"action", {{"type", "string"}, {"enum", {"write", "read", "query", "delete"}}}},
                {"key", {{"type", "string"}}},
                {"value", {{"type", "string"}}},
                {"type", {{"type", "string"}, {"enum", {"fact", "knowledge"}}}},
                {"filter", {{"type", "object"}}}
            }},
            {"required", {"action"}}
        }}
    });

    // Consolidated Write Tool
    tools.push_back({
        {"name", "write"},
        {"description", "Unified code write entry. Provide a WriteRequest with path + mode + reason. "
                        "mode is one of: insert/replace/delete/diff/bulk. "
                        "Runtime normalizes partial inputs and enforces safety rules. Legacy fields (operation/search/replace/edits) are accepted but discouraged."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "Relative path to the file"}}},
                {"reason", {{"type", "string"}, {"description", "Why this write is necessary"}}},
                {"priority", {{"type", "string"}, {"enum", {"low", "normal", "high"}}}},
                {"mode", {
                    {"type", "object"},
                    {"properties", {
                        {"type", {{"type", "string"}, {"enum", {"insert", "replace", "delete", "diff", "bulk"}}}},
                        {"start", {{"type", "integer"}}},
                        {"end", {{"type", "integer"}}},
                        {"symbol", {{"type", "string"}}},
                        {"content", {{"type", "string"}}},
                        {"search", {{"type", "string"}}},
                        {"replace", {{"type", "string"}}},
                        {"edits", {
                            {"type", "array"},
                            {"items", {
                                {"type", "object"},
                                {"properties", {
                                    {"path", {{"type", "string"}}},
                                    {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}}},
                                    {"start_line", {{"type", {"integer", "null"}}}},
                                    {"end_line", {{"type", {"integer", "null"}}}},
                                    {"content", {{"type", "string"}}}
                                }},
                                {"required", {"path", "operation", "start_line"}}
                            }}
                        }}
                    }},
                    {"required", {"type"}}
                }},
                {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}, {"description", "Line edit type for surgical modification"}}},
                {"start_line", {{"type", {"integer", "null"}}, {"description", "Start line for surgical edit"}}},
                {"end_line", {{"type", {"integer", "null"}}, {"description", "End line for surgical edit"}}},
                {"search", {{"type", "string"}, {"description", "Text to find for diff-style surgical edit"}}},
                {"replace", {{"type", "string"}, {"description", "Text to replace for diff-style surgical edit"}}},
                {"content", {{"type", "string"}, {"description", "Content for the edit or full file content"}}},
                {"edits", {
                    {"type", "array"},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}}},
                            {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}}},
                            {"start_line", {{"type", {"integer", "null"}}}},
                            {"end_line", {{"type", {"integer", "null"}}}},
                            {"content", {{"type", "string"}}}
                        }},
                        {"required", {"path", "operation", "start_line"}}
                    }}
                }}
            }},
            {"required", {"mode", "reason"}}
        }}
    });

    // Consolidated Explore Tool
    tools.push_back({
        {"name", "explore"},
        {"description", "Explore project structure. Supports file searching and directory tree listing."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "Search query for files"}}},
                {"path", {{"type", "string"}, {"description", "Directory path for tree listing"}}},
                {"depth", {{"type", "integer"}, {"description", "Tree depth"}}}
            }},
            {"required", nlohmann::json::array()}
        }}
    });

    // Diagnostics Check Tool
    tools.push_back({
        {"name", "diagnostics_check"},
        {"description", "Run build/compilation and capture errors/warnings. Automatically detects build systems. If the build fails, analyze the output to perform self-healing by fixing the reported issues. Use this after making changes to verify correctness."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"command", {{"type", "string"}, {"description", "Optional build command (if not provided, will be auto-detected)"}}}
            }},
            {"required", nlohmann::json::array()}
        }}
    });

    // Python Sandbox Tool
    tools.push_back({
        {"name", "python_sandbox"},
        {"description", "Execute transient Python code and get the output. NOTE: This tool is for short-lived, synchronous tasks. It has a 30s timeout. If execution fails, analyze the returned Traceback to fix and heal the code. For long-running tasks, use 'schedule' or 'bash_execute'."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"code", {{"type", "string"}, {"description", "The Python code to execute"}}},
                {"inputs", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"description", "Optional list of pre-filled inputs for the 'input()' function. If provided, 'input()' will consume these values sequentially. If empty or exhausted, 'input()' will raise an EOFError."}
                }}
            }},
            {"required", {"code"}}
        }}
    });

    // Pip Install Tool
    tools.push_back({
        {"name", "pip_install"},
        {"description", "Install python packages using pip. Automatically uses the detected virtual environment if available."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"package", {{"type", "string"}, {"description", "The package name(s) to install"}}}
            }},
            {"required", {"package"}}
        }}
    });

    tools.push_back({
        {"name", "arch_visualize"},
        {"description", "Generate system architecture diagrams as Mermaid markdown. Returns a Mermaid code block as plain text (no image generation)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"mermaid_content", {{"type", "string"}, {"description", "Mermaid diagram content (without code fences)."}}},
                {"title", {{"type", "string"}, {"description", "Optional title for the diagram."}}}
            }},
            {"required", {"mermaid_content"}}
        }}
    });

    tools.push_back({
        {"name", "bash_execute"},
        {"description", "Execute a bash command in the workspace. NOTE: Interactive prompts (e.g., y/n) are NOT supported and will cause the command to fail. Use non-interactive flags (e.g., 'apt-get -y'). For periodic or delayed tasks, use 'schedule'."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"command", {{"type", "string"}, {"description", "The bash command to run"}}}
            }},
            {"required", {"command"}}
        }}
    });

    // Git Operations Tool
    tools.push_back({
        {"name", "git_operations"},
        {"description", "Perform common git operations like status, diff, and log."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"operation", {{"type", "string"}, {"enum", {"status", "diff", "log", "commit"}}, {"description", "The git operation to perform"}}},
                {"message", {{"type", "string"}, {"description", "The commit message (required for commit operation)"}}}
            }},
            {"required", {"operation"}}
        }}
    });

    // Web Fetch Tool
    tools.push_back({
        {"name", "web_fetch"},
        {"description", "Fetch content from a URL (e.g., documentation, API). Note: Only supports HTTPS/HTTP."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"url", {{"type", "string"}, {"description", "The full URL to fetch"}}}
            }},
            {"required", {"url"}}
        }}
    });

    // Web Search Tool
    tools.push_back({
        {"name", "web_search"},
        {"description", "Search the web for information using DuckDuckGo (no API key required)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "The search query"}}}
            }},
            {"required", {"query"}}
        }}
    });

    // HarmonyOS Search Tool
    tools.push_back({
        {"name", "harmony_search"},
        {"description", "Search HarmonyOS official developer documentation. "
                        "IMPORTANT: This tool has a narrow scope (official docs only). If it returns no results or irrelevant snippets, "
                        "DO NOT retry. Immediately switch to `web_search` for broader community answers or `web_fetch` for deep reading."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "The keyword to search for (e.g., '画中画', 'ArkTS')"}}}
            }},
            {"required", {"query"}}
        }}
    });

    // File Undo Tool
    tools.push_back({
        {"name", "file_undo"},
        {"description", "Undo the last change made to a specific file using the structured backup in .photon/backups."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}}
            }},
            {"required", {"path"}}
        }}
    });

    // Skill Read Tool
    tools.push_back({
        {"name", "skill_read"},
        {"description", "Read the full content of a specialized skill."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}, {"description", "The name of the skill to read (e.g., 'skill-creator')"}}}
            }},
            {"required", {"name"}}
        }}
    });

    // List Tasks Tool
    tools.push_back({
        {"name", "tasks"},
        {"description", "List all active background scheduled tasks and their log paths."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", nlohmann::json::object()},
            {"required", nlohmann::json::array()}
        }}
    });

    // Cancel Task Tool
    tools.push_back({
        {"name", "cancel_task"},
        {"description", "Cancel a running background task by ID."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"task_id", {{"type", "string"}, {"description", "The ID of the task to cancel"}}}
            }},
            {"required", {"task_id"}}
        }}
    });

    // Read Task Log Tool
    tools.push_back({
        {"name", "read_task_log"},
        {"description", "Read the logs of a background task for diagnosis and self-healing analysis."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"task_id", {{"type", "string"}, {"description", "The ID of the task"}}},
                {"lines", {{"type", "integer"}, {"description", "Number of recent lines to read (default: 50)"}}}
            }},
            {"required", {"task_id"}}
        }}
    });

    // OS Timed Task Tool
    tools.push_back({
        {"name", "schedule"},
        {"description", "Schedule a background task (one-time or periodic). Use this for monitoring, reminders, or recurring maintenance. Features: 1) Log capture for failure analysis, 2) Persistent task tracking. If a task fails, use 'read_task_log' to diagnose and then reschedule if needed. For immediate system commands, use 'bash_execute'."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"delay_seconds", {{"type", "integer"}, {"description", "Delay in seconds before execution (or interval for periodic)"}}},
                {"type", {{"type", "string"}, {"enum", {"notify", "execute"}}, {"description", "Task type"}}},
                {"payload", {{"type", "string"}, {"description", "Notification message or shell command"}}},
                {"is_periodic", {{"type", "boolean"}, {"description", "If true, the task repeats every delay_seconds"}}},
                {"task_id", {{"type", "string"}, {"description", "Optional unique identifier. If an active task with this ID exists, it will be replaced."}}}
            }},
            {"required", {"delay_seconds", "type", "payload"}}
        }}
    });

    // Resolve Relative Date Tool
    tools.push_back({
        {"name", "resolve_relative_date"},
        {"description", "Resolve fuzzy date strings like 'today', 'yesterday', '2 days ago' into absolute YYYY-MM-DD format."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"fuzzy_date", {{"type", "string"}, {"description", "The fuzzy date string to resolve"}}}
            }},
            {"required", {"fuzzy_date"}}
        }}
    });

    // Project Overview Tool
    tools.push_back({
        {"name", "project_overview"},
        {"description", "Get a high-level overview of the project structure and key components. Use this to identify the main entry point (e.g., main(), app.py) and core modules."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", nlohmann::json::object()},
            {"required", nlohmann::json::array()}
        }}
    });

    // LSP Definition Tool
    tools.push_back({
        {"name", "lsp_definition"},
        {"description", "Find the definition of a symbol at a given file location using LSP (semantic navigation)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path of the file"}}},
                {"line", {{"type", "integer"}, {"description", "The line number (1-indexed)"}}},
                {"character", {{"type", "integer"}, {"description", "The character position (0-indexed)"}}}
            }},
            {"required", {"path", "line", "character"}}
        }}
    });

    tools.push_back({
        {"name", "lsp_references"},
        {"description", "Find all references (usages) of a symbol at a specific location using LSP. Essential for understanding call chains and impact analysis."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path of the file"}}},
                {"line", {{"type", "integer"}, {"description", "The line number (1-indexed)"}}},
                {"character", {{"type", "integer"}, {"description", "The character position (0-indexed)"}}}
            }},
            {"required", {"path", "line", "character"}}
        }}
    });

    return {{"result", {{"tools", tools}}}};
}

nlohmann::json InternalMCPClient::callTool(const std::string& name, const nlohmann::json& arguments) {
    ensurePhotonDirs(); // Ensure .photon directory exists before any tool call
    auto it = toolHandlers.find(name);
    if (it == toolHandlers.end()) {
        return {{"error", "Tool not found: " + name}};
    }

    if (name == "read") {
        auto hasReadArgs = [&](const nlohmann::json& args) {
            return args.contains("mode") || args.contains("query") || args.contains("requests") ||
                   args.contains("start_line") || args.contains("end_line") ||
                   args.contains("center_line") || args.contains("path");
        };
        if (enforceContextPlan && plannedReads.empty() && hasReadArgs(arguments)) {
            return {{"content", {{{"type", "text"},
                {"text", "⚠️  请先使用 plan 生成检索计划，再执行 read。"}}}}};
        }
    }
    if (name == "search") {
        auto hasSearchArgs = [&](const nlohmann::json& args) {
            return args.contains("query") || args.contains("type");
        };
        if (enforceContextPlan && plannedReads.empty() && hasSearchArgs(arguments)) {
            return {{"content", {{{"type", "text"},
                {"text", "⚠️  请先使用 plan 生成检索计划，再执行 search。"}}}}};
        }
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    nlohmann::json result = it->second(arguments);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Add Telemetry to the result
    if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
        if (name == "search" || name == "plan" || name == "lsp_definition" ||
            name == "lsp_references" || name == "explore") {
            allowNextRead = true;
        }
        if (name == "read") {
            bool didRead = !(result.contains("error"));
            if (didRead) {
                allowNextRead = false;
                if (arguments.contains("path")) {
                    readPaths.insert(arguments.value("path", ""));
                } else if (arguments.contains("requests")) {
                    for (const auto& req : arguments["requests"]) {
                        if (req.contains("path")) {
                            readPaths.insert(req.value("path", ""));
                        }
                    }
                }
                if (!lastReadTarget.empty()) {
                    readPaths.insert(lastReadTarget);
                }
                if (result["content"][0].contains("text")) {
                    std::string currentText = result["content"][0]["text"];
                    currentText += "\n\n💡 阅读完成后可简要总结，便于后续定位与修改。";
                    result["content"][0]["text"] = currentText;
                }
            }
        }
        std::string telemetry = "\n\n[Telemetry] Execution time: " + std::to_string(duration) + "ms";
        if (result["content"][0].contains("text")) {
            std::string currentText = result["content"][0]["text"];
            result["content"][0]["text"] = currentText + telemetry;
        }
    }

    auto shouldTrack = [&](const std::string& tool) {
        return tool == "read" || tool == "search" || tool == "write" || tool == "plan" || tool == "reason";
    };
    if (shouldTrack(name)) {
        std::string summary;
        if (result.contains("error")) {
            summary = result["error"].get<std::string>();
        } else if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
            const auto& item = result["content"][0];
            if (item.contains("text")) summary = item["text"].get<std::string>();
        } else {
            summary = result.dump();
        }
        if (summary.size() > 500) summary = summary.substr(0, 500) + "...";
        nlohmann::json entry = {
            {"tool", name},
            {"args", arguments},
            {"summary", summary},
            {"timestamp", static_cast<long long>(std::time(nullptr))},
            {"snapshot", {
                {"last_read", lastReadTarget},
                {"read_paths", static_cast<int>(readPaths.size())},
                {"planned_reads", static_cast<int>(plannedReads.size())},
                {"last_plan_query", lastPlanQuery}
            }}
        };
        historyEntries.push_back(entry);
        if (historyEntries.size() > maxHistoryEntries) {
            historyEntries.erase(historyEntries.begin());
        }
    }

    return result;
}

nlohmann::json InternalMCPClient::authorizeSession(const nlohmann::json& args) {
    sessionAuthorized = true;
    return {{"content", {{{"type", "text"}, {"text", "✅ 授权成功。在本次对话中，您现在可以执行写文件和 Bash 命令等高风险操作。"}}}}};
}

nlohmann::json InternalMCPClient::fileSearch(const nlohmann::json& args) {
    std::string query = args["query"];
    std::vector<std::string> matches;

    // Convert query to lower case for case-insensitive fallback
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    // Use Git if available for much faster file listing (including untracked files)
    if (isGitRepo) {
        std::string cmd = "git -C " + rootPath.u8string() + " ls-files --cached --others --exclude-standard";
        std::string output = executeCommand(cmd);
        std::stringstream ss(output);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            
            std::string lowerLine = line;
            std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
            
            if (lowerLine.find(lowerQuery) != std::string::npos) {
                matches.push_back(line);
            }
        }
        if (!matches.empty()) {
            return {{"content", {{{"type", "text"}, {"text", nlohmann::json(matches).dump(2)}}}}};
        }
    }

    try {
        auto iter = fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied);
        for (const auto& entry : iter) {
            if (shouldIgnore(entry.path())) {
                if (entry.is_directory()) iter.disable_recursion_pending();
                continue;
            }
            
            std::string relPath = fs::relative(entry.path(), rootPath).generic_string();
            std::string lowerPath = relPath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
            
            if (entry.is_regular_file()) {
                if (lowerPath.find(lowerQuery) != std::string::npos) {
                    matches.push_back(relPath);
                }
            }
        }
    } catch (const std::exception& e) {
        return {{"error", std::string("Search failed: ") + e.what()}};
    }

    return {{"content", {{{"type", "text"}, {"text", nlohmann::json(matches).dump(2)}}}}};
}

std::string InternalMCPClient::calculateHash(const std::string& content) {
    // 简单的哈希实现，用于检测内容变动
    std::hash<std::string> hasher;
    return std::to_string(hasher(content));
}

nlohmann::json InternalMCPClient::fileRead(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: File not found: " + relPathStr}}}}};
    }
    if (!fs::is_regular_file(fullPath)) {
        if (fs::is_directory(fullPath)) {
            return {{"content", {{{"type", "text"},
                {"text", "❌ READ_FAIL: Path is a directory, not a file: " + relPathStr}}}}};
        }
        return {{"content", {{{"type", "text"},
            {"text", "❌ READ_FAIL: Path is not a regular file: " + relPathStr}}}}};
    }

    // Size check for token optimization
    std::uintmax_t fileSize = fs::file_size(fullPath);
    const std::uintmax_t MAX_READ_SIZE = 50000; // 50KB soft limit

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: Could not open file: " + relPathStr}}}}};
    }

    std::string contentStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // 记录读取时的哈希值，用于后续冲突检测
    fileReadHashes[relPathStr] = calculateHash(contentStr);

    if (fileSize > MAX_READ_SIZE) {
        // Optimization: Instead of just first 2000 chars, provide a summary of symbols + first/last parts
        std::string report = "✅ READ_OK (TRUNCATED)\n";
        report += "⚠️ WARNING: File is too large (" + std::to_string(fileSize / 1024) + " KB).\n";
        report += "To save tokens, providing a structural summary. Use 'read' with mode range for details.\n\n";
        
        if (symbolManager) {
            auto syms = symbolManager->getFileSymbols(relPathStr);
            if (!syms.empty()) {
                report += "--- FILE SYMBOLS ---\n";
                for (const auto& s : syms) {
                    report += "- [" + s.type + "] " + s.name + " (Line " + std::to_string(s.line) + ")\n";
                }
                report += "\n";
            }
        }

        // Re-open file to read head/tail since we already read it all into contentStr
        std::ifstream file2(fullPath);
        if (file2.is_open()) {
            char head[1001];
            file2.read(head, 1000);
            head[file2.gcount()] = '\0';
            report += "--- BEGINNING OF FILE ---\n" + std::string(head) + "\n...\n";
            
            if (fileSize > 2000) {
                file2.seekg(-1000, std::ios::end);
                char tail[1001];
                file2.read(tail, 1000);
                tail[file2.gcount()] = '\0';
                report += "--- END OF FILE ---\n" + std::string(tail);
            }
            file2.close();
        }

        return {{"content", {{{"type", "text"}, {"text", report}}}}};
    }

    if (contentStr.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "⚠️ READ_EMPTY: File is empty: " + relPathStr}}}}};
    }
    std::string out = "✅ READ_OK\n";
    size_t lineCount = 0;
    for (char c : contentStr) { if (c == '\n') ++lineCount; }
    if (lineCount > 200) {
        out += "💡 Prefer symbol or range for large files next time (this file has " + std::to_string(lineCount) + " lines).\n\n";
    }
    out += sanitizeUtf8(contentStr, 100000);
    return {{"content", {{{"type", "text"}, {"text", out}}}}};
}

std::string InternalMCPClient::autoDetectBuildCommand() {
    // 1. Custom scripts
#ifdef _WIN32
    if (fs::exists(rootPath / "build.bat")) return "build.bat";
#else
    if (fs::exists(rootPath / "build.sh")) return "./build.sh";
#endif

    // 2. CMake (Check for build dir or just use cmake --build .)
    if (fs::exists(rootPath / "CMakeLists.txt")) {
        if (fs::exists(rootPath / "build")) return "cmake --build build";
        return "cmake --build .";
    }

    // 3. Makefile
    if (fs::exists(rootPath / "Makefile") || fs::exists(rootPath / "makefile")) {
        return "make";
    }

    // 4. Node.js
    if (fs::exists(rootPath / "package.json")) {
        // Try to read package.json to see if 'build' script exists
        try {
            std::ifstream f(rootPath / "package.json");
            nlohmann::json j;
            f >> j;
            if (j.contains("scripts") && j["scripts"].contains("build")) {
                return "npm run build";
            }
        } catch (...) {}
        return "npm install"; // Fallback to install if no build script
    }

    // 5. Rust
    if (fs::exists(rootPath / "Cargo.toml")) {
        return "cargo build";
    }

    // 6. Go
    if (fs::exists(rootPath / "go.mod")) {
        return "go build ./...";
    }

    return "";
}

nlohmann::json InternalMCPClient::diagnosticsCheck(const nlohmann::json& args) {
    std::string buildCmd = args.value("command", "");
    if (buildCmd.empty()) {
        buildCmd = autoDetectBuildCommand();
        if (buildCmd.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ Error: Could not automatically detect a build system (no CMakeLists.txt, Makefile, package.json, etc. found).\n"
                                                              "Please provide a specific command via the 'command' argument."}}}}};
        }
    }

    std::string output = executeCommand("cd \"" + rootPath.u8string() + "\" && " + buildCmd + " 2>&1");
    
    // Simple heuristic to extract errors/warnings
    std::stringstream ss(output);
    std::string line;
    std::string filtered;
    bool hasIssues = false;
    
    while (std::getline(ss, line)) {
        // Match common error patterns (gcc, clang, msvc, python, etc.)
        if (line.find("error:") != std::string::npos || 
            line.find("warning:") != std::string::npos ||
            line.find("Error ") != std::string::npos ||
            line.find("Exception") != std::string::npos ||
            line.find("Traceback") != std::string::npos) {
            filtered += line + "\n";
            hasIssues = true;
        }
    }

    if (!hasIssues) {
        return {{"content", {{{"type", "text"}, {"text", "✅ Build successful. No errors or warnings found.\n\nFull Output:\n" + output}}}}};
    }

    return {{"content", {{{"type", "text"}, {"text", "❌ Build issues found:\n\n" + filtered + "\n\n💡 Healing Strategy: Analyze the error patterns above. Use 'search' to locate the problematic code. If the error is in a file you recently modified, review your changes using 'git_operations' (diff) and apply a fix."}}}}};
}

nlohmann::json InternalMCPClient::fileWrite(const nlohmann::json& args) {
    auto processSingleWrite = [this](const nlohmann::json& writeArgs) -> nlohmann::json {
        std::string relPathStr = writeArgs["path"];
        fs::path fullPath = rootPath / fs::u8path(relPathStr);

        // Security Check: High-risk path authorization
        if (isHighRiskPath(relPathStr) && !sessionAuthorized) {
            return {
                {"status", "requires_confirmation"},
                {"is_high_risk", true},
                {"path", relPathStr},
                {"content", {{{"type", "text"}, {"text", "⚠️ 安全拦截：修改核心文件（" + relPathStr + "）属于高风险操作，需要您的显式授权。"}}}}
            };
        }

        // Conflict Detection
        if (fs::exists(fullPath)) {
            std::ifstream currentFile(fullPath);
            if (currentFile.is_open()) {
                std::string currentContent((std::istreambuf_iterator<char>(currentFile)), std::istreambuf_iterator<char>());
                currentFile.close();
                
                std::string currentHash = calculateHash(currentContent);
                if (fileReadHashes.count(relPathStr) && fileReadHashes[relPathStr] != currentHash) {
                    return {{"error", "CONFLICT DETECTED: The file '" + relPathStr + "' has been modified by someone else since you last read it."}};
                }
            }
        }

        try {
            backupFile(relPathStr);
            fs::create_directories(fullPath.parent_path());

            std::string finalContent;
            if (writeArgs.contains("operation")) {
                // Line-based editing
                std::string op = writeArgs["operation"];
                int startLine = writeArgs.value("start_line", 1);
                int endLine = writeArgs.value("end_line", startLine);
                std::string newContent = writeArgs.value("content", "");

                std::vector<std::string> lines;
                if (fs::exists(fullPath)) {
                    std::ifstream in(fullPath);
                    std::string line;
                    while (std::getline(in, line)) lines.push_back(line);
                }

                if (op == "insert") {
                    if (startLine > (int)lines.size() + 1) startLine = (int)lines.size() + 1;
                    if (startLine < 1) startLine = 1;
                    lines.insert(lines.begin() + startLine - 1, newContent);
                } else if (op == "replace") {
                    if (startLine < 1) startLine = 1;
                    if (endLine > (int)lines.size()) endLine = (int)lines.size();
                    if (startLine <= endLine) {
                        lines.erase(lines.begin() + startLine - 1, lines.begin() + endLine);
                        lines.insert(lines.begin() + startLine - 1, newContent);
                    }
                } else if (op == "delete") {
                    if (startLine < 1) startLine = 1;
                    if (endLine > (int)lines.size()) endLine = (int)lines.size();
                    if (startLine <= endLine) {
                        lines.erase(lines.begin() + startLine - 1, lines.begin() + endLine);
                    }
                }

                for (size_t i = 0; i < lines.size(); ++i) {
                    finalContent += lines[i] + (i == lines.size() - 1 ? "" : "\n");
                }
            } else if (writeArgs.contains("search") && writeArgs.contains("replace")) {
                // Search and Replace (Diff-style)
                std::string searchStr = writeArgs["search"];
                std::string replaceStr = writeArgs["replace"];
                
                std::ifstream in(fullPath);
                std::string fullContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                size_t pos = fullContent.find(searchStr);
                if (pos == std::string::npos) {
                    return {{"error", "Search string not found in " + relPathStr}};
                }
                
                // Check for uniqueness
                if (fullContent.find(searchStr, pos + 1) != std::string::npos) {
                    return {{"error", "Search string is not unique in " + relPathStr + ". Please provide more context."}};
                }

                fullContent.replace(pos, searchStr.length(), replaceStr);
                finalContent = fullContent;
            } else {
                // Full write
                finalContent = writeArgs["content"];
            }

            std::ofstream out(fullPath);
            if (!out.is_open()) return {{"error", "Could not open file for writing: " + relPathStr}};
            out << finalContent;
            out.close();

            // Update hash after write
            fileReadHashes[relPathStr] = calculateHash(finalContent);

            return {{"status", "success", {"path", relPathStr}}};
        } catch (const std::exception& e) {
            return {{"error", e.what()}};
        }
    };

    if (args.contains("edits") && args["edits"].is_array()) {
        nlohmann::json results = nlohmann::json::array();
        for (const auto& edit : args["edits"]) {
            results.push_back(processSingleWrite(edit));
        }
        return {{"content", {{{"type", "text"}, {"text", "Batch edit completed."}, {"results", results}}}}};
    } else {
        nlohmann::json res = processSingleWrite(args);
        if (res.contains("error")) return res;
        if (res.contains("status") && res["status"] == "requires_confirmation") return res;
        return {{"content", {{{"type", "text"}, {"text", "Successfully modified " + args["path"].get<std::string>()}}}}};
    }
}

bool InternalMCPClient::isCommandSafe(const std::string& cmd) {
    std::string lowerCmd = cmd;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    // 1. Dangerous Commands Blacklist
    static const std::vector<std::string> dangerous = {
        "rm ", "rd ", "del ", "format ", "mkfs", "dd ", "> /dev/", 
        "shutdown", "reboot", "passwd", "chown", "chmod", "sudo ", 
        "runas ", "net user", "net localgroup", "reg ", "kill "
    };

    for (const auto& d : dangerous) {
        if (lowerCmd.find(d) != std::string::npos) return false;
    }

    // 2. Path Escape Detection
    if (lowerCmd.find("../") != std::string::npos || lowerCmd.find("..\\") != std::string::npos) {
        return false;
    }

    // 3. Sensitive System Dirs (Cross-platform)
    static const std::vector<std::string> systemDirs = {
        "/etc/", "/dev/", "/proc/", "/sys/", "/var/", "/root/",
        "c:\\windows", "c:\\program files"
    };

    for (const auto& dir : systemDirs) {
        if (lowerCmd.find(dir) != std::string::npos) return false;
    }

    return true;
}

bool InternalMCPClient::isHighRiskCommand(const std::string& cmd) {
    std::string lowerCmd = cmd;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    // 1. 修改/删除类操作
    static const std::vector<std::string> highRisk = {
        "pip ", "npm ", "git commit", "git push", "rm ", "mv ", "cp ", 
        "mkdir ", "touch ", "sed ", "awk ", "tee ", ">", ">>"
    };

    for (const auto& h : highRisk) {
        if (lowerCmd.find(h) != std::string::npos) return true;
    }

    return false;
}

bool InternalMCPClient::isHighRiskPath(const std::string& relPath) {
    // 任何目录下的修改写入都需要确认
    return true;
}

nlohmann::json InternalMCPClient::pythonSandbox(const nlohmann::json& args) {
    std::string code = args["code"];
    
    // Heuristic: Check if the code seems to be starting a persistent server
    if ((code.find("app.run") != std::string::npos || code.find("run_flask") != std::string::npos || 
         code.find("threading.Thread") != std::string::npos) && code.find("time.sleep") != std::string::npos) {
        return {{"content", {{{"type", "text"}, {"text", "💡 Suggestion: It looks like you are trying to start a long-running server inside the transient python_sandbox.\n"
                                                          "This sandbox will exit immediately after the main script finishes, which will kill your threads/subprocesses.\n"
                                                          "To run a background service, please use 'bash_execute' with 'nohup python app.py &' and verify it using 'web_fetch'."}}}}};
    }

    // 允许更灵活的 Python 执行，但仍保留基本系统保护
    if (!isCommandSafe(code)) {
        return {{"content", {{{"type", "text"}, {"text", "Security Alert: Python code contains potentially dangerous system patterns."}}}}};
    }
    
    // Create a temporary python file in the root path
    std::string tmpFileName = ".tmp_photon_sandbox_" + std::to_string(std::time(nullptr)) + ".py";
    fs::path tmpFilePath = rootPath / tmpFileName;
    
    std::ofstream out(tmpFilePath);
    if (!out) {
        return {{"content", {{{"type", "text"}, {"text", "Error: Failed to create temporary script in " + rootPath.u8string()}}}}};
    }
    // Inject header to handle pre-filled inputs or disable interactive input
    out << "import sys, os\n";
    out << "try:\n";
    out << "    import builtins\n";
    
    if (args.contains("inputs") && args["inputs"].is_array() && !args["inputs"].empty()) {
        out << "    _prefilled_inputs = [";
        for (size_t i = 0; i < args["inputs"].size(); ++i) {
            std::string val = args["inputs"][i].get<std::string>();
            // Basic escaping for single quotes
            size_t pos = 0;
            while ((pos = val.find("'", pos)) != std::string::npos) {
                val.replace(pos, 1, "\\'");
                pos += 2;
            }
            out << "'" << val << "'" << (i == args["inputs"].size() - 1 ? "" : ", ");
        }
        out << "]\n";
        out << "    def mocked_input(prompt=''):\n";
        out << "        if _prefilled_inputs:\n";
        out << "            return _prefilled_inputs.pop(0)\n";
        out << "        raise EOFError('Photon Sandbox: No more pre-filled inputs available.')\n";
    } else {
        out << "    def mocked_input(prompt=''): raise EOFError('Photon Sandbox does not support interactive input. Use command-line arguments instead.')\n";
    }
    
    out << "    builtins.input = mocked_input\n";
    out << "except:\n";
    out << "    pass\n\n";
    out << code;
    out.close();

    std::string pythonCmd = "python3";
    
    // Try to detect virtual environment
    std::vector<std::string> venvDirs = {".venv", "venv", "env"};
    for (const auto& venv : venvDirs) {
        fs::path venvPath = rootPath / venv;
#ifdef _WIN32
        fs::path pythonExe = venvPath / "Scripts" / "python.exe";
#else
        fs::path pythonExe = venvPath / "bin" / "python";
#endif
        if (fs::exists(pythonExe)) {
            pythonCmd = "\"" + pythonExe.u8string() + "\"";
            break;
        }
    }

#ifdef _WIN32
    if (pythonCmd == "python3") {
        // Fallback for Windows
        if (executeCommand("python3 --version").find("not found") != std::string::npos || 
            executeCommand("python3 --version").empty()) {
            pythonCmd = "python";
        }
    }
    // Use a timeout to prevent hanging (e.g., 30 seconds)
    std::string fullCmd = "cd /d \"" + rootPath.u8string() + "\" && powershell -Command \"$p = Start-Process " + pythonCmd + " -ArgumentList '" + tmpFileName + "' -NoNewWindow -PassThru; if ($p.WaitForExit(30000)) { $p.ExitCode } else { Stop-Process -Id $p.Id; throw 'Timeout' }\" 2>&1";
#else
    // Use 'timeout' command on POSIX to prevent hanging
    std::string fullCmd = "cd \"" + rootPath.u8string() + "\" && timeout 30s " + pythonCmd + " \"" + tmpFileName + "\" 2>&1";
#endif

    std::string output = executeCommand(fullCmd);
    
    // Clean up
    if (fs::exists(tmpFilePath)) {
        fs::remove(tmpFilePath);
    }

    if (output.empty()) {
        output = "(Execution successful, but produced no output)";
    } else if (output.find("Timeout") != std::string::npos || output.find("terminated") != std::string::npos) {
        output = "⚠️ Execution Timed Out: The script was killed after 30 seconds to prevent hanging. For long-running tasks, use 'schedule' or 'bash_execute' with backgrounding.";
    } else if (output.find("EOFError: Photon Sandbox does not support interactive input") != std::string::npos) {
        output = "❌ Interactive Input Detected: The script tried to use 'input()', which is not allowed in this environment. \n\n💡 Healing Strategy: Rewrite the script to use command-line arguments (sys.argv) or hardcoded variables instead of interactive prompts.";
    } else if (output.find("EOFError: Photon Sandbox: No more pre-filled inputs available.") != std::string::npos) {
        output = "❌ Pre-filled Input Exhausted: The script called 'input()' more times than the provided 'inputs' array. \n\n💡 Healing Strategy: Provide more values in the 'inputs' array or modify the script to require fewer inputs.";
    } else if (output.find("Error") != std::string::npos || output.find("Exception") != std::string::npos || output.find("Traceback") != std::string::npos) {
        output = "❌ Execution Failed with errors:\n" + output + "\n\n💡 Healing Strategy: Analyze the Traceback above to identify the root cause (e.g., SyntaxError, ImportError, Logic Error). Fix the code and re-run. If it's a persistent environment issue, use 'pip_install' or 'bash_execute'.";
    }

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::pipInstall(const nlohmann::json& args) {
    std::string package = args["package"];
    std::string pipCmd = "pip3";

    // Try to detect virtual environment pip
    std::vector<std::string> venvDirs = {".venv", "venv", "env"};
    for (const auto& venv : venvDirs) {
        fs::path venvPath = rootPath / venv;
#ifdef _WIN32
        fs::path pipExe = venvPath / "Scripts" / "pip.exe";
#else
        fs::path pipExe = venvPath / "bin" / "pip";
#endif
        if (fs::exists(pipExe)) {
            pipCmd = "\"" + pipExe.u8string() + "\"";
            break;
        }
    }

    if (pipCmd == "pip3") {
#ifdef _WIN32
        if (executeCommand("pip3 --version").find("not found") != std::string::npos || 
            executeCommand("pip3 --version").empty()) {
            pipCmd = "pip";
        }
#endif
    }

    std::string fullCmd = pipCmd + " install " + package + " --no-input 2>&1";
#ifndef _WIN32
    fullCmd = "export PIP_NO_INPUT=1 && " + fullCmd + " </dev/null";
#else
    fullCmd = "set PIP_NO_INPUT=1 && " + fullCmd + " <nul";
#endif
    std::string output = executeCommand(fullCmd);

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::archVisualize(const nlohmann::json& args) {
    std::string mermaid = args.value("mermaid_content", "");
    if (mermaid.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: mermaid_content is required."}}}}};
    }

    std::string title = args.value("title", "");
    std::string output = "```mermaid\n" + mermaid + "\n```\n";
    if (!title.empty()) {
        output = "# " + title + "\n\n" + output;
    }

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::bashExecute(const nlohmann::json& args) {
    std::string command = args["command"];
    if (isHighRiskCommand(command) && !sessionAuthorized) {
        return {
            {"status", "requires_confirmation"},
            {"is_high_risk", true},
            {"content", {{{"type", "text"}, {"text", "⚠️ 安全拦截：该命令（" + command + "）属于高风险操作，需要您的显式授权。请告知用户需要点击授权按钮或调用 'authorize' 工具来开启本次对话的执行权限。"}}}}
        };
    }
    
    // Level 2 Logging: Log normal commands
    Logger::getInstance().action("Executing Bash command: " + command);
    
    if (!isCommandSafe(command)) {
        return {{"error", "Security Alert: Command blocked by Photon Guard. Restricted keywords or paths detected."}};
    }

    // Optimization: Only redirect stdin if the user hasn't provided their own input source (pipe or redirect)
    bool hasInputSource = (command.find("<") != std::string::npos || command.find("|") != std::string::npos);

    std::string shellCmd;
#ifdef _WIN32
    if (commandExists("sh")) {
        shellCmd = "sh -c \"" + command + "\"" + (hasInputSource ? "" : " <nul") + " 2>&1";
    } else {
        shellCmd = "cmd /c " + command + (hasInputSource ? "" : " <nul") + " 2>&1";
    }
#else
    shellCmd = command + (hasInputSource ? "" : " </dev/null") + " 2>&1";
#endif

    std::string output = executeCommand(shellCmd);

    // Check if the command failed because it expected input
    if (output.find("EOF") != std::string::npos || output.find("broken pipe") != std::string::npos || 
        output.find("not a tty") != std::string::npos || output.find("Input/output error") != std::string::npos) {
        output = "❌ Interactive Command Detected: The command tried to read from stdin (e.g., a y/n prompt or password input), but stdin is disabled to prevent hanging.\n\n" + output + 
                 "\n\n💡 Healing Strategy: Use non-interactive flags (e.g., '-y', '--force', '--batch') or pipe the input directly (e.g., 'echo \"password\" | command').";
    }

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::codeAstAnalyze(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) return {{"content", {{{"type", "text"}, {"text", "Error: File not found"}}}}};

    std::string result = "## Structure of " + relPathStr + "\n\n";
    
    if (symbolManager) {
        auto syms = symbolManager->getFileSymbols(relPathStr);
        if (!syms.empty()) {
            result += "### 🧩 Code Structure (via Tree-sitter/Index)\n\n";
            for (const auto& s : syms) {
                std::string lineInfo = "Line " + std::to_string(s.line);
                if (s.endLine > s.line) {
                    lineInfo += "-" + std::to_string(s.endLine);
                }
                
                std::string typeIcon = "🔹";
                if (s.type == "class" || s.type == "struct") typeIcon = "📦";
                else if (s.type == "function" || s.type == "method") typeIcon = "⚙️";
                
                result += "- " + typeIcon + " **[" + s.type + "]** `" + s.name + "` (" + lineInfo + ")\n";
            }
            result += "\n*Tip: Use 'read' with mode range for precise reading.*\n";
            return {{"content", {{{"type", "text"}, {"text", result}}}}};
        }
    }

    // Fallback to Regex if SymbolManager is not available or returned nothing
    result += "### 🔍 Code Structure (via Regex Fallback)\n\n";
    std::ifstream file(fullPath);
    std::string line;
    int lineNum = 0;

    // More robust regex for C++ and Python
    static const std::regex cppClass(R"raw((class|struct)\s+([A-Za-z0-9_]+)(\s*:\s*[^{]+)?\s*\{)raw");
    static const std::regex cppFunc(R"raw(([A-Za-z0-9_<>, :*&]+)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*(const|override|final|noexcept)*\s*(\{|;))raw");
    static const std::regex pyDef(R"raw(^\s*(def|async def)\s+([A-Za-z0-9_]+)\s*\()raw");
    static const std::regex pyClass(R"raw(^\s*class\s+([A-Za-z0-9_]+)\s*[:\(])raw");

    while (std::getline(file, line)) {
        lineNum++;
        // Simple comment skipping
        if (line.find("//") != std::string::npos && line.find("//") < 5) continue;
        if (line.find("#") != std::string::npos && line.find("#") < 5) continue;

        std::smatch match;
        if (std::regex_search(line, match, cppClass) || std::regex_search(line, match, pyClass)) {
            result += "- **[Class]** `" + match[match.size() > 1 ? (match.size() == 2 ? 1 : 2) : 0].str() + "` (Line " + std::to_string(lineNum) + ")\n";
        } else if (std::regex_search(line, match, cppFunc)) {
            result += "  - **[Method]** `" + match[2].str() + "` (Line " + std::to_string(lineNum) + ") -> Returns `" + match[1].str() + "`\n";
        } else if (std::regex_search(line, match, pyDef)) {
            result += "  - **[Function]** `" + match[2].str() + "` (Line " + std::to_string(lineNum) + ")\n";
        }
    }
    
    result += "\n*Tip: Use 'read' with mode range for precise reading.*";
    return {{"content", {{{"type", "text"}, {"text", result}}}}};
}

nlohmann::json InternalMCPClient::gitOperations(const nlohmann::json& args) {
    std::string op = args["operation"];
    std::string rootPathStr = rootPath.u8string();
    std::string cmd = "git -C " + rootPathStr + " ";
    
    if (op == "status") cmd += "status";
    else if (op == "diff") cmd += "diff";
    else if (op == "log") cmd += "log --oneline -n 10";
    else if (op == "commit") {
        if (!args.contains("message")) return {{"error", "Commit message required"}};
        cmd += "add . && git -C " + rootPathStr + " commit -m \"" + args["message"].get<std::string>() + "\"";
    }

    std::string output = executeCommand(cmd + " 2>&1");
    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::webFetch(const nlohmann::json& args) {
    std::string url = args["url"];
    
    // Simple URL parser
    static const std::regex urlRegex(R"raw((https?)://([^/]+)(/.*)?)raw");
    std::smatch match;
    if (!std::regex_match(url, match, urlRegex)) {
        return {{"error", "Invalid URL format. Use http(s)://host/path"}};
    }

    std::string scheme = match[1];
    std::string host = match[2];
    std::string path = match[3].length() > 0 ? match[3].str() : "/";

    try {
        std::string baseUrl = scheme + "://" + host;
        httplib::Client cli(baseUrl);
        cli.set_follow_location(true);
        cli.set_connection_timeout(10);
        cli.set_read_timeout(15);
        
        // Add User-Agent to avoid being blocked by some sites
        httplib::Headers headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
        };
        
        auto res = cli.Get(path.c_str(), headers);

        if (res && res->status == 200) {
            std::string cleaned = htmlToMarkdown(res->body);
            std::string body = sanitizeUtf8(cleaned, 30000); 
            if (body.length() < 100 && res->body.length() > 500) {
                body = sanitizeUtf8(cleanHtml(res->body), 30000);
            }
            return {{"content", {{{"type", "text"}, {"text", body}}}}};
        } else {
            return {{"error", "Failed to fetch (" + (res ? std::to_string(res->status) : "No Response") + "): " + url}};
        }
    } catch (const std::exception& e) {
        return {{"error", std::string("Fetch exception: ") + e.what()}};
    }
}

nlohmann::json InternalMCPClient::webSearch(const nlohmann::json& args) {
    std::string query = args["query"];
    
    auto trySearch = [&](const std::string& host, const std::string& pathTemplate) -> nlohmann::json {
        try {
            httplib::Client cli("https://" + host);
            cli.set_follow_location(true);
            cli.set_decompress(true);
            cli.set_connection_timeout(10);
            cli.set_read_timeout(15);
            
            httplib::Headers headers = {
                {"User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"},
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8"},
                {"Accept-Language", "en-US,en;q=0.5"}
            };
            
            std::string path = pathTemplate;
            size_t qpos = path.find("{q}");
            if (qpos != std::string::npos) path.replace(qpos, 3, urlEncode(query));

            auto res = cli.Get(path.c_str(), headers);
            if (!res || res->status != 200) return nullptr;

            std::string html = res->body;
            if (html.find("bots use DuckDuckGo too") != std::string::npos || html.find("CAPTCHA") != std::string::npos) {
                return nullptr; // Blocked by CAPTCHA
            }

            std::string results = "Search Results (" + host + ") for: " + query + "\n\n";
            int count = 0;

            if (host.find("duckduckgo") != std::string::npos) {
                // Fix: Use standard string with manual escaping to avoid R-literal issues
                static const std::regex entry_regex("<a[^>]*class=\"[^\"]*result__a[^\"]*\"[^>]*href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>[\\s\\S]*?(?:<a[^>]*class=\"[^\"]*result__snippet[^\"]*\"[^>]*>([\\s\\S]*?)</a>)?");
                auto words_begin = std::sregex_iterator(html.begin(), html.end(), entry_regex);
                auto words_end = std::sregex_iterator();
                for (std::sregex_iterator i = words_begin; i != words_end && count < 5; ++i) {
                    std::smatch match = *i;
                    std::string link = match[1].str();
                    size_t uddg_pos = link.find("uddg=");
                    if (uddg_pos != std::string::npos) {
                        link = urlDecode(link.substr(uddg_pos + 5));
                        size_t amp_pos = link.find("&");
                        if (amp_pos != std::string::npos) link = link.substr(0, amp_pos);
                    }
                    results += "### [" + cleanHtml(match[2].str()) + "](" + link + ")\n" + cleanHtml(match[3].str()) + "\n\n";
                    count++;
                }
            } else if (host.find("bing") != std::string::npos) {
                // Bing Mobile/Lite regex
                static const std::regex bing_regex(R"raw(<li class="b_algo"><h2><a href="([^"]+)"[^>]*>([\s\S]*?)</a></h2>[\s\S]*?<div class="b_caption"><p[^>]*>([\s\S]*?)</p>)raw");
                auto words_begin = std::sregex_iterator(html.begin(), html.end(), bing_regex);
                auto words_end = std::sregex_iterator();
                for (std::sregex_iterator i = words_begin; i != words_end && count < 5; ++i) {
                    std::smatch match = *i;
                    results += "### [" + cleanHtml(match[2].str()) + "](" + match[1].str() + ")\n" + cleanHtml(match[3].str()) + "\n\n";
                    count++;
                }
            }

            if (count > 0) return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 10000)}}}}};
            return nullptr;
        } catch (...) { return nullptr; }
    };

    // Primary: DuckDuckGo
    nlohmann::json res = trySearch("html.duckduckgo.com", "/html/?q={q}");
    if (!res.is_null()) return res;

    // Secondary: Bing
    res = trySearch("www.bing.com", "/search?q={q}");
    if (!res.is_null()) return res;

    return {{"error", "Search failed: All engines blocked the request or triggered CAPTCHA.\n\n"
                      "💡 **Fallback Strategy**: External search is currently unavailable. Please DO NOT stay in a search loop. "
                      "Instead, use `search` to find similar patterns in the local codebase, "
                      "or synthesize the solution based on existing knowledge and documentation already read."}};
}

nlohmann::json InternalMCPClient::harmonySearch(const nlohmann::json& args) {
    std::string query = args["query"];
    
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif
    ss << std::put_time(&time_info, "%Y%m%d%H%M%S");
    std::string ts = ss.str();

    nlohmann::json body = {
        {"deviceId", "ESN"}, {"deviceType", "1"}, {"language", "zh"}, {"country", "CN"},
        {"keyWord", query}, {"requestOrgin", 5}, {"ts", ts},
        {"developerVertical", {
            {"category", 1}, {"language", "zh"}, {"catalog", "harmonyos-guides"}, 
            {"searchSubTitle", 0}, {"scene", 2}, {"subType", 4}
        }},
        {"cutPage", {{"offset", 0}, {"length", 12}}}
    };

    try {
        httplib::Client cli("https://svc-drcn.developer.huawei.com");
        cli.set_follow_location(true);
        cli.set_decompress(true);
        cli.set_connection_timeout(5); // 5 seconds timeout
        cli.set_read_timeout(5);       // 5 seconds timeout
        
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36"}
        };

        auto res = cli.Post("/community/servlet/consumer/partnerCommunityService/developer/search", headers, body.dump(), "application/json");
        if (res && res->status == 200) {
            auto j = nlohmann::json::parse(res->body);
            std::string results = "HarmonyOS Search Results for: " + query + "\n\n";
            
            if (j.contains("searchResult") && j["searchResult"].is_array() && !j["searchResult"].empty()) {
                auto& firstResult = j["searchResult"][0];
                if (firstResult.contains("developerInfos") && firstResult["developerInfos"].is_array() && !firstResult["developerInfos"].empty()) {
                    int count = 0;
                    for (const auto& item : firstResult["developerInfos"]) {
                        if (count >= 8) break;
                        
                        std::string title = item.value("name", item.value("title", "Untitled"));
                        std::string urlPart = item.value("url", "");
                        std::string link = urlPart;
                        
                        if (!urlPart.empty()) {
                            if (urlPart.find("//") == 0) {
                                link = "https:" + urlPart;
                            } else if (urlPart.find("http") != 0) {
                                link = std::string("https://developer.huawei.com") + (urlPart[0] == '/' ? "" : "/") + urlPart;
                            }
                        }

                        std::string snippet = cleanHtml(item.value("description", item.value("content", "")));
                        results += "### [" + title + "](" + link + ")\n" + snippet + "\n\n";
                        count++;
                    }
                    results += "\n---\n💡 **Note**: If the above snippets do not contain the specific code or answer you need, DO NOT repeat harmony_search. Instead, use `web_fetch` on the most relevant link above, or use `web_search` for broader community solutions.";
                    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 20000)}}}}};
                }
            }
            return {{"content", {{{"type", "text"}, {"text", "❌ No results found in HarmonyOS official database for: " + query + "\n\n**Action Required**: Official docs may be missing this specific topic. Please switch to `web_search` to find community solutions or GitHub examples immediately."}}}}};
        }
    } catch (...) {}
    return {{"error", "HarmonyOS search failed due to network or parsing error"}};
}

nlohmann::json InternalMCPClient::readFileLines(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    int start = args["start_line"];
    int end = args["end_line"];
    bool includeRaw = args.value("include_raw", false);
    bool compressOnly = args.value("compress", true);
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: File not found: " + relPathStr}}}}};
    }

    if (start < 1) start = 1;
    if (end < start) end = start;
    const int MAX_RANGE_LINES = 200;
    const int MAX_FILE_BUDGET = 800;
    int requestedLines = end - start + 1;
    if (requestedLines > MAX_RANGE_LINES) {
        return {{"content", {{{"type", "text"},
            {"text", "❌ READ_FAIL: 读取范围过大（" + std::to_string(requestedLines) +
                     " 行）。请缩小范围或使用 read(mode:range/symbol)。"}}}}};
    }
    int usedLines = fileReadLineCounts[relPathStr];
    if (usedLines + requestedLines > MAX_FILE_BUDGET) {
        return {{"content", {{{"type", "text"},
            {"text", "❌ READ_FAIL: 单文件读取预算已达上限（" + std::to_string(MAX_FILE_BUDGET) +
                     " 行）。请改用搜索或符号定位。"}}}}};
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: Could not open file: " + relPathStr}}}}};
    }

    std::string line;
    std::string content;
    content += "📄 File: `" + relPathStr + "` | Lines " + std::to_string(start) + "-" + std::to_string(end);
    if (args.contains("label")) {
        content += " | Focus: " + args["label"].get<std::string>();
    }
    content += "\n";
    std::string separator;
    for(int i=0; i<60; ++i) separator += "─";
    content += separator + "\n";
    
    int current = 1;
    int totalLines = 0;
    int fileTotalLines = 0;
    int controlCount = 0;
    int errorCount = 0;
    int ioCount = 0;
    int dbCount = 0;
    int validateCount = 0;
    int commentSkipped = 0;
    int logSkipped = 0;
    int returnCount = 0;
    std::vector<std::string> controlSamples;
    std::vector<std::string> errorSamples;
    std::vector<std::string> ioSamples;
    std::vector<std::string> dbSamples;
    std::vector<std::string> validateSamples;
    std::vector<std::string> returnSamples;
    std::vector<std::pair<int, std::string>> rawLines;
    std::vector<std::pair<int, std::string>> compressedLines;
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    auto addSample = [](std::vector<std::string>& samples, const std::string& s) {
        if (samples.size() < 5) samples.push_back(s);
    };
    auto ltrim = [](const std::string& s) {
        size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        return s.substr(i);
    };
    auto isCommentLine = [&](const std::string& s) {
        std::string t = ltrim(s);
        return t.rfind("//", 0) == 0 || t.rfind("/*", 0) == 0 || t.rfind("*", 0) == 0 || t.rfind("#", 0) == 0;
    };
    auto isLogLine = [&](const std::string& lower) {
        return lower.find("log") != std::string::npos ||
               lower.find("printf") != std::string::npos ||
               lower.find("std::cout") != std::string::npos ||
               lower.find("std::cerr") != std::string::npos ||
               lower.find("console.") != std::string::npos;
    };
    auto isTrivialReturn = [&](const std::string& s) {
        std::string t = ltrim(s);
        if (t.rfind("return ", 0) != 0) return false;
        if (t.find(";") == std::string::npos) return false;
        if (t.size() > 80) return false;
        return t.find("(") == std::string::npos;
    };
    while (std::getline(file, line)) {
        fileTotalLines++;
        if (current >= start && current <= end) {
            totalLines++;
            if (!compressOnly && includeRaw) {
                rawLines.push_back({current, line});
            }

            std::string lower = toLower(line);
            if (lower.find("if") != std::string::npos || lower.find("switch") != std::string::npos ||
                lower.find("for") != std::string::npos || lower.find("while") != std::string::npos ||
                lower.find("try") != std::string::npos || lower.find("catch") != std::string::npos) {
                controlCount++;
                addSample(controlSamples, std::to_string(current) + " | " + line);
            }
            if (lower.find("throw") != std::string::npos || lower.find("error") != std::string::npos ||
                lower.find("exception") != std::string::npos || lower.find("fail") != std::string::npos ||
                lower.find("return err") != std::string::npos || lower.find("return error") != std::string::npos) {
                errorCount++;
                addSample(errorSamples, std::to_string(current) + " | " + line);
            }
            if (lower.find("return") != std::string::npos) {
                returnCount++;
                addSample(returnSamples, std::to_string(current) + " | " + line);
            }
            if (lower.find("read") != std::string::npos || lower.find("write") != std::string::npos ||
                lower.find("open") != std::string::npos || lower.find("close") != std::string::npos ||
                lower.find("http") != std::string::npos || lower.find("request") != std::string::npos ||
                lower.find("response") != std::string::npos || lower.find("socket") != std::string::npos) {
                ioCount++;
                addSample(ioSamples, std::to_string(current) + " | " + line);
            }
            if (lower.find("sql") != std::string::npos || lower.find("query") != std::string::npos ||
                lower.find("insert") != std::string::npos || lower.find("update") != std::string::npos ||
                lower.find("delete") != std::string::npos || lower.find("repo") != std::string::npos ||
                lower.find("db") != std::string::npos) {
                dbCount++;
                addSample(dbSamples, std::to_string(current) + " | " + line);
            }
            if (lower.find("validate") != std::string::npos || lower.find("check") != std::string::npos ||
                lower.find("verify") != std::string::npos || lower.find("sanitize") != std::string::npos) {
                validateCount++;
                addSample(validateSamples, std::to_string(current) + " | " + line);
            }

            if (isCommentLine(line)) {
                commentSkipped++;
            } else if (isLogLine(lower)) {
                logSkipped++;
            } else {
                bool relevant = false;
                if (controlCount > 0 && (lower.find("if") != std::string::npos || lower.find("switch") != std::string::npos ||
                    lower.find("for") != std::string::npos || lower.find("while") != std::string::npos ||
                    lower.find("try") != std::string::npos || lower.find("catch") != std::string::npos)) {
                    relevant = true;
                }
                if (lower.find("return") != std::string::npos || lower.find("throw") != std::string::npos ||
                    lower.find("error") != std::string::npos || lower.find("exception") != std::string::npos) {
                    relevant = true;
                }
                if (lower.find("(") != std::string::npos && lower.find(")") != std::string::npos) {
                    if (lower.find("if") == std::string::npos && lower.find("for") == std::string::npos &&
                        lower.find("while") == std::string::npos && lower.find("switch") == std::string::npos) {
                        relevant = true;
                    }
                }
                if (lower.find("=") != std::string::npos && lower.find(";") != std::string::npos) {
                    relevant = true;
                }
                if (!isTrivialReturn(line) && relevant) {
                    compressedLines.push_back({current, line});
                }
            }
        }
        if (current > end) break;
        current++;
    }
    
    if (totalLines == 0) {
        content += "⚠️  No lines in range. ";
        if (fileTotalLines > 0) {
            content += "File has " + std::to_string(fileTotalLines) + " total line(s).\n";
            content += "💡 Tip: Use smaller ranges or read(query) for precise context.\n";
        } else {
            content += "File is empty or could not be read.\n";
        }
    } else {
        std::string separator;
        for(int i=0; i<60; ++i) separator += "─";
        content += separator + "\n";
        content += "✅ Read " + std::to_string(totalLines) + " line(s)";
        if (fileTotalLines > totalLines) {
            content += " (out of " + std::to_string(fileTotalLines) + " total)";
        }
        content += "\n";
    }

    if (totalLines > 0) {
        if (compressOnly) {
            content += "\n💡 当前为压缩视图。如需原始代码，请使用 include_raw=true 且 compress=false。\n";
        } else if (!includeRaw) {
            content += "\n💡 默认只输出压缩视图与结构化摘要。如需原始代码，请使用 include_raw=true。\n";
        }
    }

    if (!compressOnly && includeRaw && !rawLines.empty()) {
        content += "\n原始视图（按需展开）:\n";
        int shown = 0;
        for (const auto& p : rawLines) {
            if (shown++ >= 200) {
                content += "  ...（已截断）\n";
                break;
            }
            content += "  " + std::to_string(p.first) + " | " + p.second + "\n";
        }
    }

    if (totalLines > 0) {
        content += "\n结构化摘要（自动标签）:\n";
        if (controlCount > 0) {
            content += "- 控制流: " + std::to_string(controlCount) + " 处\n";
            for (const auto& s : controlSamples) content += "  • " + s + "\n";
        }
        if (errorCount > 0) {
            content += "- 错误处理: " + std::to_string(errorCount) + " 处\n";
            for (const auto& s : errorSamples) content += "  • " + s + "\n";
        }
        if (validateCount > 0) {
            content += "- 校验/检查: " + std::to_string(validateCount) + " 处\n";
            for (const auto& s : validateSamples) content += "  • " + s + "\n";
        }
        if (returnCount > 0) {
            content += "- 返回语句: " + std::to_string(returnCount) + " 处\n";
            for (const auto& s : returnSamples) content += "  • " + s + "\n";
        }
        if (dbCount > 0) {
            content += "- 数据/存储: " + std::to_string(dbCount) + " 处\n";
            for (const auto& s : dbSamples) content += "  • " + s + "\n";
        }
        if (ioCount > 0) {
            content += "- I/O/网络: " + std::to_string(ioCount) + " 处\n";
            for (const auto& s : ioSamples) content += "  • " + s + "\n";
        }

        content += "\n压缩视图（去噪后关键行）:\n";
        content += "- 注释略过: " + std::to_string(commentSkipped) + " 行\n";
        content += "- 日志略过: " + std::to_string(logSkipped) + " 行\n";
        if (compressedLines.empty()) {
            content += "  (无可压缩关键行)\n";
        } else {
            int shown = 0;
            for (const auto& p : compressedLines) {
                if (shown++ >= 50) {
                    content += "  ...（已截断）\n";
                    break;
                }
                content += "  " + std::to_string(p.first) + " | " + p.second + "\n";
            }
        }

        // 结构化压缩器：轻量 JSON 摘要
        auto listToJsonArray = [](const std::vector<std::string>& items) {
            std::string out = "[";
            for (size_t i = 0; i < items.size(); ++i) {
                if (i) out += ", ";
                out += "\"" + items[i] + "\"";
            }
            out += "]";
            return out;
        };
        std::vector<std::string> focusTags;
        if (controlCount > 0) focusTags.push_back("control_flow");
        if (errorCount > 0) focusTags.push_back("error_handling");
        if (validateCount > 0) focusTags.push_back("validation");
        if (dbCount > 0) focusTags.push_back("data_store");
        if (ioCount > 0) focusTags.push_back("io_network");

        // 尝试补充语义字段（基于包裹符号）
        std::string symbolName;
        std::string symbolType;
        std::string symbolSignature;
        std::vector<std::string> callHints;
        if (symbolManager) {
            auto enclosing = symbolManager->findEnclosingSymbol(relPathStr, start);
            if (enclosing.has_value()) {
                symbolName = enclosing->name;
                symbolType = enclosing->type;
                symbolSignature = enclosing->signature;
                auto calls = symbolManager->getCallsForSymbol(enclosing.value());
                int shown = 0;
                for (const auto& c : calls) {
                    if (shown++ >= 8) break;
                    callHints.push_back(c.name);
                }
            }
        }
        std::vector<std::string> sideEffects;
        if (dbCount > 0) sideEffects.push_back("data_store");
        if (ioCount > 0) sideEffects.push_back("io_network");
        std::vector<std::string> errorSignals = errorSamples;
        std::vector<std::string> inputs;
        std::string outputType;
        auto trim = [](std::string s) {
            size_t startPos = 0;
            while (startPos < s.size() && std::isspace(static_cast<unsigned char>(s[startPos]))) startPos++;
            size_t endPos = s.size();
            while (endPos > startPos && std::isspace(static_cast<unsigned char>(s[endPos - 1]))) endPos--;
            return s.substr(startPos, endPos - startPos);
        };
        if (!symbolSignature.empty()) {
            size_t l = symbolSignature.find('(');
            size_t r = symbolSignature.find(')');
            if (l != std::string::npos && r != std::string::npos && r > l) {
                std::string params = symbolSignature.substr(l + 1, r - l - 1);
                std::stringstream ss(params);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    std::string t = trim(item);
                    if (!t.empty()) inputs.push_back(t);
                }
                std::string tail = symbolSignature.substr(r + 1);
                size_t arrow = tail.find("->");
                if (arrow != std::string::npos) {
                    outputType = trim(tail.substr(arrow + 2));
                } else {
                    size_t colon = tail.find(':');
                    if (colon != std::string::npos) {
                        outputType = trim(tail.substr(colon + 1));
                    }
                }
            }
        }

        content += "\n结构化压缩器（JSON 摘要）:\n";
        content += "{\n";
        content += "  \"file\": \"" + relPathStr + "\",\n";
        content += "  \"range\": \"" + std::to_string(start) + "-" + std::to_string(end) + "\",\n";
        if (!symbolName.empty()) {
            content += "  \"symbol\": {\n";
            content += "    \"name\": \"" + symbolName + "\",\n";
            content += "    \"type\": \"" + symbolType + "\"";
            if (!symbolSignature.empty()) {
                content += ",\n    \"signature\": \"" + symbolSignature + "\"\n";
            } else {
                content += "\n";
            }
            content += "  },\n";
        }
        content += "  \"focus\": " + listToJsonArray(focusTags) + ",\n";
        content += "  \"signals\": {\n";
        content += "    \"control_flow\": " + std::to_string(controlCount) + ",\n";
        content += "    \"error_handling\": " + std::to_string(errorCount) + ",\n";
        content += "    \"validation\": " + std::to_string(validateCount) + ",\n";
        content += "    \"data_store\": " + std::to_string(dbCount) + ",\n";
        content += "    \"io_network\": " + std::to_string(ioCount) + "\n";
        content += "  },\n";
        if (!callHints.empty()) {
            content += "  \"calls\": " + listToJsonArray(callHints) + ",\n";
        }
        if (!inputs.empty()) {
            content += "  \"inputs\": " + listToJsonArray(inputs) + ",\n";
        }
        if (!outputType.empty()) {
            content += "  \"output\": \"" + outputType + "\",\n";
        }
        if (!sideEffects.empty()) {
            content += "  \"side_effects\": " + listToJsonArray(sideEffects) + ",\n";
        }
        if (!errorSignals.empty()) {
            content += "  \"error_signals\": " + listToJsonArray(errorSignals) + ",\n";
        }
        content += "  \"evidence\": {\n";
        content += "    \"control_flow\": " + listToJsonArray(controlSamples) + ",\n";
        content += "    \"error_handling\": " + listToJsonArray(errorSamples) + ",\n";
        content += "    \"validation\": " + listToJsonArray(validateSamples) + ",\n";
        content += "    \"data_store\": " + listToJsonArray(dbSamples) + ",\n";
        content += "    \"io_network\": " + listToJsonArray(ioSamples) + "\n";
        content += "  }\n";
        content += "}\n";
    }

    if (!enforceContextPlan && plannedReads.empty()) {
        content += "\n💡 建议先使用 plan 生成检索计划，以更接近 Claude Code 的分层检索方式。\n";
    }

    std::string nextQuery;
    if (enforceContextPlan && symbolManager) {
        auto enclosing = symbolManager->findEnclosingSymbol(relPathStr, start);
        if (enclosing.has_value()) {
            content += "\n🔎 Enclosing symbol: `" + enclosing->name + "` (" + enclosing->type + ")\n";
            auto calls = symbolManager->getCallsForSymbol(enclosing.value());
            if (!calls.empty()) {
                content += "Next-step call hints:\n";
                int shown = 0;
                int bestScore = -1;
                for (const auto& c : calls) {
                    if (shown++ >= 5) break;
                    content += "- " + c.name + " (line " + std::to_string(c.line) + ")\n";
                    int score = symbolManager->getGlobalCalleeCount(c.name);
                    if (score > bestScore) {
                        bestScore = score;
                        nextQuery = c.name;
                    }
                }
                if (nextQuery.empty() && !calls.empty()) {
                    nextQuery = calls.front().name;
                }
                content += "→ 建议对以上调用点使用 plan(query=\"<call>\") 继续分层推进。\n";
            }
        }
    }

    if (enforceContextPlan && autoPlanEnabled && !nextQuery.empty()) {
        nlohmann::json planArgs = {
            {"query", nextQuery},
            {"max_entries", 5},
            {"max_calls", 10}
        };
        auto planRes = contextPlan(planArgs);
        if (planRes.contains("content") && planRes["content"].is_array() && !planRes["content"].empty()) {
            const auto& item = planRes["content"][0];
            if (item.contains("type") && item["type"] == "text" && item.contains("text")) {
                content += "\n\nAUTO NEXT PLAN\n";
                content += item["text"].get<std::string>();
                content += "\n";
            }
        }
    }
    
    fileReadLineCounts[relPathStr] = usedLines + totalLines;
    if (fileTotalLines == 0 || totalLines == 0) {
        return {{"content", {{{"type", "text"}, {"text",
            "⚠️ READ_EMPTY: File has no content in requested range: " + relPathStr +
            " (" + std::to_string(start) + "-" + std::to_string(end) + ")"}}}}};
    }
    return {{"content", {{{"type", "text"}, {"text", "✅ READ_OK\n" + sanitizeUtf8(content, 8000)}}}}};
}

nlohmann::json InternalMCPClient::readBatchLines(const nlohmann::json& args) {
    if (!args.contains("requests") || !args["requests"].is_array()) {
        return {{"error", "Missing or invalid 'requests' array"}};
    }

    if (enforceContextPlan && plannedReads.empty()) {
        return {{"content", {{{"type", "text"},
            {"text", "⚠️  请先使用 plan 生成检索计划，再执行批量读取。"}}}}};
    }

    bool compressOnly = args.value("compress", true);
    bool includeRaw = args.value("include_raw", false);

    std::string report;
    int totalFilesProcessed = 0;

    for (const auto& req : args["requests"]) {
        std::string relPathStr = req.value("path", "");
        int start = req.value("start_line", 1);
        int end = req.value("end_line", 1);

        if (enforceContextPlan && !plannedReads.empty()) {
            bool matched = false;
            for (const auto& pr : plannedReads) {
                if (pr.path != relPathStr) continue;
                if (end >= pr.startLine && start <= pr.endLine) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                std::string hint = "⚠️  批量读取包含计划外范围，请先更新 plan。\n";
                hint += "可用范围示例：\n";
                int shown = 0;
                for (const auto& pr : plannedReads) {
                    if (shown++ >= 3) break;
                    hint += "- " + pr.path + " " + std::to_string(pr.startLine) + "-" + std::to_string(pr.endLine) + " (" + pr.label + ")\n";
                }
                return {{"content", {{{"type", "text"}, {"text", hint}}}}};
            }
        }

        bool reqCompress = req.value("compress", compressOnly);
        bool reqIncludeRaw = req.value("include_raw", includeRaw);
        if (reqCompress || reqIncludeRaw) {
            nlohmann::json merged = req;
            merged["compress"] = reqCompress;
            merged["include_raw"] = reqIncludeRaw;
            auto res = readFileLines(merged);
            if (res.contains("content") && res["content"].is_array() && !res["content"].empty()) {
                const auto& item = res["content"][0];
                if (item.contains("text")) {
                    report += item["text"].get<std::string>() + "\n";
                    totalFilesProcessed++;
                }
            }
            continue;
        }
        
        fs::path fullPath = rootPath / fs::u8path(relPathStr);
        if (!fs::exists(fullPath)) {
            report += "❌ READ_FAIL: File not found: " + relPathStr + "\n\n";
            continue;
        }

        std::ifstream file(fullPath);
        if (!file.is_open()) {
            report += "❌ READ_FAIL: Could not open file: " + relPathStr + "\n\n";
            continue;
        }

        if (start < 1) start = 1;
        if (end < start) end = start;
        const int MAX_RANGE_LINES = 200;
        const int MAX_FILE_BUDGET = 800;
        int requestedLines = end - start + 1;
        if (requestedLines > MAX_RANGE_LINES) {
            report += "❌ READ_FAIL: 读取范围过大（" + std::to_string(requestedLines) +
                      " 行）：`" + relPathStr + "`. 请缩小范围或使用 read(query).\n\n";
            continue;
        }
        int usedLines = fileReadLineCounts[relPathStr];
        if (usedLines + requestedLines > MAX_FILE_BUDGET) {
            report += "❌ READ_FAIL: 单文件读取预算已达上限（" + std::to_string(MAX_FILE_BUDGET) +
                      " 行）：`" + relPathStr + "`. 请改用搜索或符号定位。\n\n";
            continue;
        }

        report += "📄 File: `" + relPathStr + "` | Lines " + std::to_string(start) + "-" + std::to_string(end) + "\n";
        std::string separator;
        for(int i=0; i<60; ++i) separator += "─";
        report += separator + "\n";

        std::string line;
        int current = 1;
        int linesRead = 0;
        while (std::getline(file, line)) {
            if (current >= start && current <= end) {
                report += std::to_string(current) + " | " + line + "\n";
                linesRead++;
            }
            if (current > end) break;
            current++;
        }
        
        if (linesRead == 0) {
            report += "⚠️ READ_EMPTY: No lines found in this range.\n";
        }
        report += separator + "\n\n";
        totalFilesProcessed++;
        fileReadLineCounts[relPathStr] = usedLines + linesRead;
    }

    if (report.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "⚠️ READ_EMPTY: No content read."}}}}};
    }
    return {{"content", {{{"type", "text"}, {"text", "✅ READ_OK (BATCH)\n" + sanitizeUtf8(report, 12000)}}}}};
}

nlohmann::json InternalMCPClient::readSymbol(const nlohmann::json& args) {
    if (!args.contains("reason") || args["reason"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: reason is required for read (symbol mode)."}}}}};
    }
    if (!symbolManager) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: SymbolManager not initialized."}}}}};
    }
    std::string symbol = args.value("symbol", "");
    if (symbol.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: symbol is required."}}}}};
    }
    std::string pathFilter = args.value("path", "");
    int contextLines = args.value("context_lines", 40);
    if (contextLines < 10) contextLines = 10;
    if (contextLines > 80) contextLines = 80;
    int half = contextLines / 2;

    auto results = symbolManager->search(symbol);
    if (!pathFilter.empty()) {
        std::vector<SymbolManager::Symbol> filtered;
        for (const auto& s : results) {
            if (s.path == pathFilter) filtered.push_back(s);
        }
        results.swap(filtered);
    }
    if (results.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: Symbol not found: " + symbol}}}}};
    }
    const auto* picked = &results.front();
    for (const auto& s : results) {
        if (s.name == symbol) { picked = &s; break; }
    }
    lastReadTarget = picked->path;
    int start = picked->line - half;
    if (start < 1) start = 1;
    int end = (picked->endLine > 0 ? picked->endLine : picked->line) + half;

    nlohmann::json readArgs = {
        {"path", picked->path},
        {"start_line", start},
        {"end_line", end},
        {"label", "symbol:" + symbol}
    };
    return readFileLines(readArgs);
}

nlohmann::json InternalMCPClient::readFileRange(const nlohmann::json& args) {
    if (!args.contains("reason") || args["reason"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: reason is required for read (range mode)."}}}}};
    }
    if (!args.contains("path") || !args.contains("start_line") || !args.contains("end_line")) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: path/start_line/end_line are required."}}}}};
    }
    int start = args["start_line"];
    int end = args["end_line"];
    if (start < 1) start = 1;
    if (end < start) end = start;
    const int MAX_RANGE = 120;
    if (end - start + 1 > MAX_RANGE) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: range too large (>120 lines). Use a smaller range or a center_line window."}}}}};
    }
    nlohmann::json readArgs = {
        {"path", args["path"]},
        {"start_line", start},
        {"end_line", end},
        {"label", "range"}
    };
    lastReadTarget = args["path"];
    return readFileLines(readArgs);
}

nlohmann::json InternalMCPClient::readContextWindow(const nlohmann::json& args) {
    if (!args.contains("reason") || args["reason"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: reason is required for read (context window)."}}}}};
    }
    if (!args.contains("path") || !args.contains("center_line")) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: path and center_line are required."}}}}};
    }
    int center = args["center_line"];
    int before = args.value("before", 20);
    int after = args.value("after", 20);
    if (before < 1) before = 1;
    if (after < 1) after = 1;
    if (before > 30) before = 30;
    if (after > 30) after = 30;
    int start = center - before;
    int end = center + after;
    if (start < 1) start = 1;
    nlohmann::json readArgs = {
        {"path", args["path"]},
        {"start_line", start},
        {"end_line", end},
        {"label", "context_window"}
    };
    lastReadTarget = args["path"];
    return readFileLines(readArgs);
}

nlohmann::json InternalMCPClient::readDeclaration(const nlohmann::json& args) {
    if (!args.contains("reason") || args["reason"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: reason is required for read (declaration window)."}}}}};
    }
    if (!symbolManager) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: SymbolManager not initialized."}}}}};
    }
    std::string symbol = args.value("symbol", "");
    if (symbol.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: symbol is required."}}}}};
    }
    std::string pathFilter = args.value("path", "");
    int contextLines = args.value("context_lines", 6);
    if (contextLines < 2) contextLines = 2;
    if (contextLines > 10) contextLines = 10;

    auto results = symbolManager->search(symbol);
    if (!pathFilter.empty()) {
        std::vector<SymbolManager::Symbol> filtered;
        for (const auto& s : results) {
            if (s.path == pathFilter) filtered.push_back(s);
        }
        results.swap(filtered);
    }
    if (results.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: Symbol not found: " + symbol}}}}};
    }
    const auto* picked = &results.front();
    for (const auto& s : results) {
        if (s.name == symbol) { picked = &s; break; }
    }
    lastReadTarget = picked->path;
    int start = picked->line - contextLines;
    if (start < 1) start = 1;
    int end = picked->line + contextLines;
    nlohmann::json readArgs = {
        {"path", picked->path},
        {"start_line", start},
        {"end_line", end},
        {"label", "declaration:" + symbol}
    };
    return readFileLines(readArgs);
}

nlohmann::json InternalMCPClient::readFileHeader(const nlohmann::json& args) {
    if (!args.contains("path")) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: path is required."}}}}};
    }
    int maxLines = args.value("max_lines", 40);
    if (maxLines < 10) maxLines = 10;
    if (maxLines > 80) maxLines = 80;
    nlohmann::json readArgs = {
        {"path", args["path"]},
        {"start_line", 1},
        {"end_line", maxLines},
        {"label", "file_header"}
    };
    lastReadTarget = args["path"];
    return readFileLines(readArgs);
}

nlohmann::json InternalMCPClient::readFullFile(const nlohmann::json& args) {
    if (!args.contains("reason") || args["reason"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: reason is required for read (full mode)."}}}}};
    }
    if (!args.contains("path")) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: path is required."}}}}};
    }
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);
    if (!fs::exists(fullPath)) {
        return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: File not found: " + relPathStr}}}}};
    }
    int lineCount = countFileLines(fullPath);
    bool declarative = isDeclarativeFile(relPathStr);
    if (!declarative && lineCount > 200) {
        return {{"content", {{{"type", "text"}, {"text",
            "❌ READ_FAIL: Full-file read is blocked for implementation files over 200 lines. "
            "Use read with mode: symbol or range instead."}}}}};
    }
    lastReadTarget = relPathStr;
    return fileRead(args);
}

nlohmann::json InternalMCPClient::listDirTree(const nlohmann::json& args) {
    std::string subPathStr = args.value("path", "");
    int maxDepth = args.value("depth", 3);
    fs::path startPath = rootPath / fs::u8path(subPathStr);

    if (!fs::exists(startPath)) return {{"content", {{{"type", "text"}, {"text", "Error: Path not found"}}}}};

    std::string tree;
    try {
        auto iter = fs::recursive_directory_iterator(startPath, fs::directory_options::skip_permission_denied);
        for (const auto& entry : iter) {
            auto rel = fs::relative(entry.path(), startPath);
            int depth = static_cast<int>(std::distance(rel.begin(), rel.end()));
            if (depth > maxDepth) continue;

            if (shouldIgnore(entry.path())) {
                if (entry.is_directory()) iter.disable_recursion_pending();
                continue;
            }

            std::string name = entry.path().filename().u8string();
            for (int i = 0; i < depth - 1; ++i) tree += "  ";
            tree += (depth > 0 ? "└── " : "") + name + (entry.is_directory() ? "/" : "") + "\n";
        }
    } catch (const std::exception& e) {
        return {{"error", std::string("List tree failed: ") + e.what()}};
    }
    return {{"content", {{{"type", "text"}, {"text", tree}}}}};
}

nlohmann::json InternalMCPClient::diffApply(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    std::string search = args["search"];
    std::string replace = args["replace"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    // Backup before modifying
    backupFile(relPathStr);

    std::ifstream inFile(fullPath);
    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // Strategy: Try exact match first, then try with trimmed search
    size_t pos = content.find(search);
    if (pos == std::string::npos) {
        // Simple fallback: try finding without trailing newlines/spaces
        std::string trimmedSearch = search;
        trimmedSearch.erase(trimmedSearch.find_last_not_of(" \n\r\t") + 1);
        pos = content.find(trimmedSearch);
        if (pos == std::string::npos) {
            return {{"error", "Search string not found in file. Ensure exact match or check line endings."}};
        }
    }
    
    // Check for uniqueness
    if (content.find(search, pos + 1) != std::string::npos) {
        return {{"error", "Search string is not unique. Provide more context."}};
    }

    content.replace(pos, search.length(), replace);

    std::ofstream outFile(fullPath);
    outFile << content;
    outFile.close();

    return {{"content", {{{"type", "text"}, {"text", "Successfully applied change to " + relPathStr}}}}};
}

nlohmann::json InternalMCPClient::fileEditLines(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    std::string operation = args["operation"];
    int startLine = args["start_line"];
    int endLine = args.value("end_line", -1);
    std::string content = args.value("content", "");
    
    fs::path fullPath = rootPath / fs::u8path(relPathStr);
    
    if (!fs::exists(fullPath)) {
        return {{"error", "File not found: " + relPathStr}};
    }
    
    if (startLine < 1) {
        return {{"error", "start_line must be >= 1 (1-based)"}};
    }
    
    // Backup before modifying
    backupFile(relPathStr);
    
    // Read all lines into vector
    std::ifstream inFile(fullPath);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();
    
    int totalLines = static_cast<int>(lines.size());
    std::string result;
    
    if (operation == "insert") {
        if (content.empty()) {
            return {{"error", "content is required for insert operation"}};
        }
        if (startLine > totalLines + 1) {
            return {{"error", "start_line (" + std::to_string(startLine) + ") exceeds file length (" + std::to_string(totalLines) + ") + 1"}};
        }
        
        // Split content into lines
        std::vector<std::string> newLines;
        std::istringstream contentStream(content);
        std::string newLine;
        while (std::getline(contentStream, newLine)) {
            newLines.push_back(newLine);
        }
        
        // Insert before startLine (0-based index is startLine - 1)
        lines.insert(lines.begin() + (startLine - 1), newLines.begin(), newLines.end());
        
        result = "✅ Inserted " + std::to_string(newLines.size()) + " line(s) before line " + std::to_string(startLine);
        if (totalLines > 0) {
            result += " (file now has " + std::to_string(static_cast<int>(lines.size())) + " lines)";
        }
        
    } else if (operation == "replace") {
        if (endLine < startLine) {
            return {{"error", "end_line must be >= start_line for replace operation"}};
        }
        if (endLine > totalLines) {
            return {{"error", "end_line (" + std::to_string(endLine) + ") exceeds file length (" + std::to_string(totalLines) + ")"}};
        }
        if (content.empty()) {
            return {{"error", "content is required for replace operation"}};
        }
        
        // Split content into lines
        std::vector<std::string> newLines;
        std::istringstream contentStream(content);
        std::string newLine;
        while (std::getline(contentStream, newLine)) {
            newLines.push_back(newLine);
        }
        
        // Replace lines from startLine to endLine (0-based: startLine-1 to endLine-1)
        int replaceCount = endLine - startLine + 1;
        lines.erase(lines.begin() + (startLine - 1), lines.begin() + endLine);
        lines.insert(lines.begin() + (startLine - 1), newLines.begin(), newLines.end());
        
        result = "✅ Replaced lines " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                 " (" + std::to_string(replaceCount) + " line(s)) with " + std::to_string(newLines.size()) + " new line(s)";
        
    } else if (operation == "delete") {
        if (endLine < startLine) {
            return {{"error", "end_line must be >= start_line for delete operation"}};
        }
        if (endLine > totalLines) {
            return {{"error", "end_line (" + std::to_string(endLine) + ") exceeds file length (" + std::to_string(totalLines) + ")"}};
        }
        
        // Delete lines from startLine to endLine (0-based: startLine-1 to endLine)
        int deleteCount = endLine - startLine + 1;
        lines.erase(lines.begin() + (startLine - 1), lines.begin() + endLine);
        
        result = "✅ Deleted lines " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                 " (" + std::to_string(deleteCount) + " line(s))";
        if (static_cast<int>(lines.size()) > 0) {
            result += " (file now has " + std::to_string(static_cast<int>(lines.size())) + " lines)";
        } else {
            result += " (file is now empty)";
        }
        
    } else {
        return {{"error", "Invalid operation. Must be 'insert', 'replace', or 'delete'"}};
    }
    
    // Write back to file
    std::ofstream outFile(fullPath);
    if (!outFile.is_open()) {
        return {{"error", "Could not open file for writing: " + relPathStr}};
    }
    
    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i < lines.size() - 1) {
            outFile << "\n";
        }
    }
    outFile.close();
    
    return {{"content", {{{"type", "text"}, {"text", result}}}}};
}

nlohmann::json InternalMCPClient::editBatchLines(const nlohmann::json& args) {
    if (!args.contains("edits") || !args["edits"].is_array()) {
        return {{"error", "Missing or invalid 'edits' array"}};
    }

    struct FileEdit {
        std::string path;
        std::string operation;
        int startLine;
        int endLine;
        std::string content;
    };

    std::map<std::string, std::vector<FileEdit>> groupedEdits;
    for (const auto& edit : args["edits"]) {
        FileEdit fe;
        fe.path = edit.value("path", "");
        fe.operation = edit.value("operation", "");
        fe.startLine = edit.value("start_line", 0);
        fe.endLine = edit.value("end_line", fe.startLine);
        fe.content = edit.value("content", "");
        
        if (fe.path.empty() || fe.operation.empty() || fe.startLine < 1) {
            return {{"error", "Invalid edit parameters in batch"}};
        }
        groupedEdits[fe.path].push_back(fe);
    }

    std::map<std::string, std::vector<std::string>> fileContents;
    for (auto const& [path, edits] : groupedEdits) {
        fs::path fullPath = rootPath / fs::u8path(path);
        if (!fs::exists(fullPath)) {
            return {{"error", "File not found: " + path}};
        }

        std::ifstream inFile(fullPath);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(inFile, line)) {
            lines.push_back(line);
        }
        inFile.close();

        // Sort edits for this file in reverse order to keep line numbers valid
        auto sortedEdits = edits;
        std::sort(sortedEdits.begin(), sortedEdits.end(), [](const FileEdit& a, const FileEdit& b) {
            return a.startLine > b.startLine;
        });

        for (const auto& fe : sortedEdits) {
            int totalLines = static_cast<int>(lines.size());
            if (fe.operation == "insert") {
                if (fe.startLine > totalLines + 1) return {{"error", "Line out of range in " + fe.path}};
                std::vector<std::string> newLines;
                std::istringstream iss(fe.content);
                std::string nl;
                while (std::getline(iss, nl)) newLines.push_back(nl);
                lines.insert(lines.begin() + (fe.startLine - 1), newLines.begin(), newLines.end());
            } else if (fe.operation == "replace") {
                if (fe.endLine > totalLines || fe.endLine < fe.startLine) return {{"error", "Line range out of bounds in " + fe.path}};
                std::vector<std::string> newLines;
                std::istringstream iss(fe.content);
                std::string nl;
                while (std::getline(iss, nl)) newLines.push_back(nl);
                lines.erase(lines.begin() + (fe.startLine - 1), lines.begin() + fe.endLine);
                lines.insert(lines.begin() + (fe.startLine - 1), newLines.begin(), newLines.end());
            } else if (fe.operation == "delete") {
                if (fe.endLine > totalLines || fe.endLine < fe.startLine) return {{"error", "Line range out of bounds in " + fe.path}};
                lines.erase(lines.begin() + (fe.startLine - 1), lines.begin() + fe.endLine);
            }
        }
        fileContents[path] = lines;
    }

    // If we reached here, all edits are valid. Now write them.
    std::string report = "✅ Atomic batch edit successful:\n";
    for (auto const& [path, lines] : fileContents) {
        backupFile(path);
        fs::path fullPath = rootPath / fs::u8path(path);
        std::ofstream outFile(fullPath);
        for (size_t i = 0; i < lines.size(); ++i) {
            outFile << lines[i] << (i == lines.size() - 1 ? "" : "\n");
        }
        outFile.close();
        report += "- Updated `" + path + "` (" + std::to_string(lines.size()) + " lines)\n";
    }

    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::fileUndo(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);
    fs::path backupPath = rootPath / ".photon" / "backups" / fs::u8path(relPathStr);

    if (!fs::exists(backupPath)) {
        return {{"error", "No backup found for file in .photon/backups: " + relPathStr}};
    }

    try {
        fs::copy_file(backupPath, fullPath, fs::copy_options::overwrite_existing);
        return {{"content", {{{"type", "text"}, {"text", "Successfully restored " + relPathStr + " from .photon/backups."}}}}};
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

void InternalMCPClient::backupFile(const std::string& relPathStr) {
    lastFile = relPathStr; // Track last modified file
    fs::path fullPath = rootPath / fs::u8path(relPathStr);
    if (fs::exists(fullPath)) {
        fs::path backupPath = rootPath / ".photon" / "backups" / fs::u8path(relPathStr);
        fs::create_directories(backupPath.parent_path());
        fs::copy_file(fullPath, backupPath, fs::copy_options::overwrite_existing);
    }
}

void InternalMCPClient::ensurePhotonDirs() {
    fs::create_directories(globalDataPath / "backups");
}

nlohmann::json InternalMCPClient::projectOverview(const nlohmann::json& args) {
    std::string report = "# Project Overview: " + rootPath.filename().u8string() + "\n\n";
    
    // 1. Core Directory Structure (Limited Depth)
    report += "## 📂 Key Directories\n";
    try {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(rootPath)) {
            if (entry.is_directory() && !shouldIgnore(entry.path())) {
                report += "- `" + entry.path().filename().u8string() + "/`\n";
                if (++count > 15) break; 
            }
        }
    } catch (...) {}

    // 2. Discover Core Symbols (Summary)
    report += "\n## 🧠 Core Components (Heuristic)\n";
    // We look for files that likely contain core logic
    std::vector<std::string> coreFiles;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().u8string();
                if (name == "main.cpp" || name == "App.js" || name == "index.ts" || 
                    name.find("Manager") != std::string::npos || name.find("Client") != std::string::npos) {
                    coreFiles.push_back(fs::relative(entry.path(), rootPath).u8string());
                }
            }
            if (coreFiles.size() > 10) break;
        }
    } catch (...) {}

    for (const auto& f : coreFiles) {
        report += "- `" + f + "`\n";
    }

    report += "\n*Strategy: Use 'reason' (type=dependency) on these core files instead of reading them all.*";
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::contextPlan(const nlohmann::json& args) {
    if (!symbolManager) {
        return {{"content", {{{"type", "text"}, {"text", "SymbolManager not initialized."}}}}};
    }

    std::string query = args.value("query", "");
    if (query.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: query is required."}}}}};
    }

    int topK = args.value("top_k", 5);
    int maxEntries = args.value("max_entries", 5);
    int maxCalls = args.value("max_calls", 10);
    bool includeSemantic = args.value("include_semantic", false);
    std::string pathFilter = args.value("path", "");

    if (topK < 1) topK = 1;
    if (maxEntries < 1) maxEntries = 1;
    if (maxCalls < 0) maxCalls = 0;

    struct Entry {
        SymbolManager::Symbol sym;
        bool fromSemantic = false;
        int score = 0;
        std::vector<std::string> reasons;
    };

    std::vector<Entry> entries;
    std::unordered_set<std::string> seen;

    auto makeKey = [](const SymbolManager::Symbol& s) {
        return s.path + ":" + std::to_string(s.line) + ":" + s.name;
    };
    auto addEntry = [&](const SymbolManager::Symbol& s, bool fromSemantic) {
        std::string key = makeKey(s);
        if (seen.count(key)) return false;
        seen.insert(key);
        entries.push_back({s, fromSemantic});
        return true;
    };

    auto symbols = symbolManager->search(query);
    if (!pathFilter.empty()) {
        std::vector<SymbolManager::Symbol> filtered;
        for (const auto& s : symbols) {
            if (s.path == pathFilter) filtered.push_back(s);
        }
        symbols.swap(filtered);
    }
    for (const auto& s : symbols) {
        addEntry(s, false);
    }

    if ((entries.empty() || includeSemantic) && semanticManager) {
        auto chunks = semanticManager->search(query, topK);
        for (const auto& chunk : chunks) {
            if (!pathFilter.empty() && chunk.path != pathFilter) continue;
            if (chunk.startLine <= 0) continue;
            auto enclosing = symbolManager->findEnclosingSymbol(chunk.path, chunk.startLine);
            if (enclosing.has_value()) {
                addEntry(enclosing.value(), true);
                continue;
            }
            SymbolManager::Symbol pseudo;
            pseudo.name = "chunk@" + std::to_string(chunk.startLine);
            pseudo.type = "chunk";
            pseudo.source = "semantic";
            pseudo.path = chunk.path;
            pseudo.line = chunk.startLine;
            pseudo.endLine = chunk.endLine;
            addEntry(pseudo, true);
        }
    }

    if (entries.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "No entry candidates found for: " + query}}}}};
    }

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    auto hasAny = [&](const std::string& hay, const std::vector<std::string>& needles) {
        for (const auto& n : needles) {
            if (hay.find(n) != std::string::npos) return true;
        }
        return false;
    };

    std::string lowerQuery = lower(query);
    bool allowUtility = (lowerQuery.find("util") != std::string::npos) ||
                        (lowerQuery.find("helper") != std::string::npos) ||
                        (pathFilter.find("util") != std::string::npos) ||
                        (pathFilter.find("helper") != std::string::npos);
    for (auto& e : entries) {
        const auto& s = e.sym;
        std::string nameLower = lower(s.name);
        std::string pathLower = lower(s.path);
        std::string fileLower = lower(fs::path(s.path).filename().string());
        std::string extLower = lower(fs::path(s.path).extension().string());

        if (nameLower == lowerQuery) {
            e.score += 4;
            e.reasons.push_back("exact_name");
        } else if (nameLower.find(lowerQuery) != std::string::npos) {
            e.score += 2;
            e.reasons.push_back("name_match");
        }

        if (s.source == "lsp") { e.score += 3; e.reasons.push_back("lsp"); }
        else if (s.source == "tree_sitter") { e.score += 2; e.reasons.push_back("ast"); }
        else if (s.source == "regex") { e.score += 1; e.reasons.push_back("regex"); }

        if (e.fromSemantic) { e.score -= 1; e.reasons.push_back("semantic_fallback"); }

        if (s.type == "function" || s.type == "method") { e.score += 2; e.reasons.push_back("callable"); }
        else if (s.type == "class" || s.type == "struct") { e.score += 1; e.reasons.push_back("type"); }

        if (!s.signature.empty()) { e.score += 1; e.reasons.push_back("signature"); }

        if (hasAny(nameLower, {"main", "init", "bootstrap", "handler", "controller", "router", "endpoint", "startup"})) {
            e.score += 2; e.reasons.push_back("entry_name");
        }
        if (hasAny(pathLower, {"/controller", "/router", "/handler", "/service", "/api", "/app", "/main", "/routes", "/server"})) {
            e.score += 2; e.reasons.push_back("entry_path");
        }
        if (hasAny(fileLower, {"main", "app", "server", "index", "router", "routes"})) {
            e.score += 2; e.reasons.push_back("entry_file");
        }
        if (!s.signature.empty() && hasAny(lower(s.signature), {"router", "endpoint", "http", "route", "handler"})) {
            e.score += 2; e.reasons.push_back("entry_signature");
        }
        if (hasAny(pathLower, {"/controllers", "/routes", "/handlers", "/endpoints", "/rest", "/rpc", "/grpc", "/graphql", "/resolvers"})) {
            e.score += 2; e.reasons.push_back("framework_path");
        }
        if (hasAny(fileLower, {"urls", "views", "controllers", "handlers", "endpoints", "routes", "router"})) {
            e.score += 2; e.reasons.push_back("framework_file");
        }
        if (hasAny(nameLower, {"handle", "handler", "controller", "route", "endpoint", "get", "post", "put", "delete"})) {
            e.score += 1; e.reasons.push_back("framework_name");
        }
        if (!s.signature.empty() && hasAny(lower(s.signature), {"request", "response", "httprequest", "httpresponse", "context", "gincontext", "fiber.ctx", "koa", "express"})) {
            e.score += 2; e.reasons.push_back("framework_signature");
        }
        if (hasAny(pathLower, {"/util", "/utils", "/helper", "/helpers", "/common", "/shared"})) {
            e.score -= 2; e.reasons.push_back("utility_path");
        }

        // Language-specific entry signals
        if (extLower == ".cpp" || extLower == ".cc" || extLower == ".cxx" || extLower == ".h" || extLower == ".hpp") {
            if (hasAny(nameLower, {"main", "wmain", "winmain"})) {
                e.score += 3; e.reasons.push_back("cpp_entry");
            }
            if (hasAny(fileLower, {"main.cpp", "main.cc", "main.cxx", "app.cpp", "app.cc"})) {
                e.score += 2; e.reasons.push_back("cpp_entry_file");
            }
            if (!s.signature.empty() && hasAny(lower(s.signature), {"int main", "void main"})) {
                e.score += 2; e.reasons.push_back("cpp_entry_signature");
            }
        } else if (extLower == ".ets" || extLower == ".ts") {
            if (hasAny(nameLower, {"entry", "ability", "page", "oncreate", "onwindowstagecreate", "onforeground", "onstart", "oninit"})) {
                e.score += 3; e.reasons.push_back("arkts_entry");
            }
            if (hasAny(pathLower, {"/entry", "/pages", "/ability", "/uiability"})) {
                e.score += 2; e.reasons.push_back("arkts_entry_path");
            }
            if (hasAny(fileLower, {"entry.ets", "index.ets", "main.ets", "app.ets"})) {
                e.score += 2; e.reasons.push_back("arkts_entry_file");
            }
            if (!s.signature.empty() && hasAny(lower(s.signature), {"ability", "uiability", "@entry", "@component"})) {
                e.score += 2; e.reasons.push_back("arkts_entry_signature");
            }
        } else if (extLower == ".py") {
            if (hasAny(fileLower, {"app.py", "main.py", "wsgi.py", "asgi.py", "urls.py", "views.py", "routes.py"})) {
                e.score += 3; e.reasons.push_back("py_entry_file");
            }
            if (hasAny(pathLower, {"/views", "/urls", "/routers"})) {
                e.score += 2; e.reasons.push_back("py_entry_path");
            }
            if (hasAny(nameLower, {"view", "handler", "endpoint"})) {
                e.score += 1; e.reasons.push_back("py_entry_name");
            }
            if (!s.signature.empty() && hasAny(lower(s.signature), {"fastapi", "flask", "django", "asgi", "wsgi", "request", "response", "router"})) {
                e.score += 2; e.reasons.push_back("py_entry_signature");
            }
        }
        if (pathLower.find("/test") != std::string::npos || pathLower.find("/spec") != std::string::npos) {
            e.score -= 2; e.reasons.push_back("test_path");
        }

        auto calls = symbolManager->getCallsForSymbol(s);
        if (!calls.empty()) {
            int boost = std::min(3, static_cast<int>(calls.size() / 5));
            if (boost > 0) {
                e.score += boost;
                e.reasons.push_back("call_rich");
            }
        }
        int calleeFreq = symbolManager->getGlobalCalleeCount(s.name);
        if (calleeFreq > 0) {
            int boost = std::min(3, calleeFreq / 5);
            if (boost > 0) {
                e.score += boost;
                e.reasons.push_back("global_callee");
            }
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.sym.path != b.sym.path) return a.sym.path < b.sym.path;
        return a.sym.line < b.sym.line;
    });
    if (static_cast<int>(entries.size()) > maxEntries) {
        entries.resize(maxEntries);
    }

    plannedReads.clear();
    lastPlanQuery = query;

    // If query mentions a file path (e.g. "读取interfaces/.../js_pip_controller.h文件内容"), add it to plannedReads
    // so that "plan then read" works even when symbol search returns nothing.
    static const std::vector<std::string> exts = { ".h", ".hpp", ".cpp", ".c", ".ets", ".ts", ".js", ".json", ".md", ".py", ".yml", ".yaml" };
    for (const auto& ext : exts) {
        size_t pos = 0;
        while ((pos = query.find(ext, pos)) != std::string::npos) {
            size_t end = pos + ext.size();
            size_t start = pos;
            while (start > 0 && query[start - 1] != ' ' && query[start - 1] != '"' && query[start - 1] != '\'') {
                --start;
            }
            std::string candidate = query.substr(start, end - start);
            size_t slash = candidate.find('/');
            if (slash != std::string::npos) {
                candidate = candidate.substr(slash);
                fs::path full = rootPath / fs::u8path(candidate);
                if (fs::exists(full) && fs::is_regular_file(full)) {
                    plannedReads.push_back({candidate, 1, 9999, "file"});
                    break;
                }
            }
            pos = end;
        }
        if (!plannedReads.empty()) break;
    }

    const int MAX_PLAN_WINDOW = 80;
    auto addPlanned = [&](const std::string& path, int start, int end, const std::string& label) {
        if (path.empty() || start <= 0) return;
        std::string lowerPath = lower(path);
        if (!allowUtility && hasAny(lowerPath, {"/util", "/utils", "/helper", "/helpers", "/common", "/shared"})) {
            return;
        }
        if (end < start) end = start;
        if (end - start + 1 > MAX_PLAN_WINDOW) {
            end = start + MAX_PLAN_WINDOW - 1;
        }
        plannedReads.push_back({path, start, end, label});
    };

    std::string report = "Context Plan for: `" + query + "`\n\n";
    report += "Stage 1 - Entry Candidates (symbol-first, ranked):\n";
    if (!entries.empty()) {
        const auto& top = entries.front();
        report += "Recommended Entry: `" + top.sym.name + "` in `" + top.sym.path + "` (score=" +
                  std::to_string(top.score) + ")\n";
    }
    for (const auto& e : entries) {
        const auto& s = e.sym;
        std::string range = (s.endLine > 0)
            ? ("Lines " + std::to_string(s.line) + "-" + std::to_string(s.endLine))
            : ("Line " + std::to_string(s.line));
        std::string sourceTag = e.fromSemantic ? "semantic" : "symbol";
        report += "• `" + s.name + "` [" + s.type + "|" + sourceTag + "] in `" + s.path + "` (" + range + ")";
        report += " | score=" + std::to_string(e.score);
        if (!e.reasons.empty()) {
            report += " | reasons: ";
            for (size_t i = 0; i < e.reasons.size(); ++i) {
                if (i) report += ", ";
                report += e.reasons[i];
            }
        }
        report += "\n";
        if (!s.signature.empty()) {
            report += "  signature: " + s.signature + "\n";
        }
        if (s.endLine > 0) {
            addPlanned(s.path, s.line, s.endLine, s.name);
        } else {
            addPlanned(s.path, std::max(1, s.line - 20), s.line + 20, s.name);
        }
    }

    if (maxCalls > 0) {
        report += "\nStage 2 - Direct Call Hints (bounded):\n";
        for (const auto& e : entries) {
            const auto& s = e.sym;
            if (s.endLine <= 0 || s.line <= 0) continue;
            auto calls = symbolManager->getCallsForSymbol(s);
            if (calls.empty()) {
                calls = symbolManager->extractCalls(s.path, s.line, s.endLine);
            }
            if (calls.empty()) continue;
            report += "• `" + s.name + "` calls:\n";
            int count = 0;
            for (const auto& c : calls) {
                if (count >= maxCalls) break;
                report += "  - " + c.name + " (line " + std::to_string(c.line) + ")\n";
                count++;
            }
        }
    }

    report += "\nSuggested Reads:\n";
    for (const auto& e : entries) {
        const auto& s = e.sym;
        if (s.endLine > 0) {
            report += "• read({mode:{type:\"range\",start:" +
                      std::to_string(std::max(1, s.line)) + ", end:" +
                      std::to_string(std::max(s.endLine, s.line)) + "}, path:\"" + s.path +
                      "\", reason:\"inspect range\"})\n";
        } else {
            report += "• read({mode:{type:\"symbol\",name:\"" + s.name + "\"}, path:\"" + s.path + "\", reason:\"inspect symbol\"})\n";
        }
    }

    report += "\n*Tip: Use this plan iteratively. If a call looks relevant, re-run plan on that symbol.*";
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::lspDefinition(const nlohmann::json& args) {
    auto extractIdentifierAt = [](const fs::path& path, int line, int character) -> std::string {
        if (line <= 0 || character < 0) return "";
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::string textLine;
        int current = 0;
        while (std::getline(file, textLine)) {
            current++;
            if (current == line) break;
        }
        if (current != line) return "";
        if (textLine.empty()) return "";
        int idx = std::min<int>(character, static_cast<int>(textLine.size()));
        auto isIdent = [](char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return std::isalnum(uc) || c == '_';
        };
        int start = idx;
        while (start > 0 && isIdent(textLine[start - 1])) {
            start--;
        }
        int end = idx;
        while (end < static_cast<int>(textLine.size()) && isIdent(textLine[end])) {
            end++;
        }
        if (start >= end) return "";
        return textLine.substr(start, end - start);
    };
    auto fallbackToSymbolIndex = [&](const std::string& reason,
                                     const std::string& relPath,
                                     int line,
                                     int character) -> nlohmann::json {
        if (!symbolManager) {
            return {{"content", {{{"type", "text"}, {"text", reason + " Symbol index not initialized."}}}}};
        }
        fs::path fullPath = fs::absolute(rootPath / fs::u8path(relPath));
        std::string ident = extractIdentifierAt(fullPath, line, character);
        if (ident.empty()) {
            return {{"content", {{{"type", "text"}, {"text", reason + " No identifier found at " + relPath + ":" + std::to_string(line)}}}}};
        }
        auto results = symbolManager->search(ident);
        if (results.empty()) {
            return {{"content", {{{"type", "text"}, {"text", reason + " No symbol matches for: " + ident}}}}};
        }
        auto toLower = [](const std::string& s) {
            std::string out = s;
            for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        };
        std::string identLower = toLower(ident);
        std::vector<SymbolManager::Symbol> localExact;
        std::vector<SymbolManager::Symbol> localLoose;
        std::vector<SymbolManager::Symbol> globalExact;
        std::vector<SymbolManager::Symbol> globalLoose;
        for (const auto& s : results) {
            bool isLocal = s.path == relPath;
            bool exact = toLower(s.name) == identLower;
            if (isLocal && exact) localExact.push_back(s);
            else if (isLocal) localLoose.push_back(s);
            else if (exact) globalExact.push_back(s);
            else globalLoose.push_back(s);
        }
        std::vector<SymbolManager::Symbol> ordered;
        ordered.reserve(results.size());
        ordered.insert(ordered.end(), localExact.begin(), localExact.end());
        ordered.insert(ordered.end(), localLoose.begin(), localLoose.end());
        ordered.insert(ordered.end(), globalExact.begin(), globalExact.end());
        ordered.insert(ordered.end(), globalLoose.begin(), globalLoose.end());

        std::string report = reason + " Fallback to symbol index for `" + ident + "`.\n";
        report += "Found " + std::to_string(ordered.size()) + " candidates:\n";
        size_t limit = std::min<size_t>(ordered.size(), 20);
        for (size_t i = 0; i < limit; ++i) {
            const auto& s = ordered[i];
            report += "- [" + s.type + "] `" + s.name + "` in `" + s.path + "` (Line " + std::to_string(s.line) + ")\n";
        }
        if (ordered.size() > limit) {
            report += "... and " + std::to_string(ordered.size() - limit) + " more\n";
        }
        return {{"content", {{{"type", "text"}, {"text", report}}}}};
    };
    auto pickClient = [&](const std::string& relPath) -> LSPClient* {
        if (!lspByExtension.empty()) {
            std::string ext = fs::path(relPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            auto it = lspByExtension.find(ext);
            if (it != lspByExtension.end()) return it->second;
        }
        return lspClient;
    };

    std::string relPath = args["path"];
    int line = args["line"];
    int character = args["character"];
    LSPClient* client = pickClient(relPath);
    if (!client) {
        return fallbackToSymbolIndex("LSP client not initialized.", relPath, line, character);
    }

    fs::path fullPath = fs::absolute(rootPath / fs::u8path(relPath));
    std::string fileUri = "file://" + fullPath.u8string();

    LSPClient::Position position{line - 1, character};
    auto locations = client->goToDefinition(fileUri, position);

    if (locations.empty()) {
        return fallbackToSymbolIndex("No definition found by LSP.", relPath, line, character);
    }

    std::string report = "Found " + std::to_string(locations.size()) + " definition(s):\n\n";
    for (size_t i = 0; i < locations.size(); ++i) {
        const auto& loc = locations[i];
        std::string locPath = loc.uri;
        if (locPath.find("file://") == 0) locPath = locPath.substr(7);
        int startLine = loc.range.start.line + 1;
        int endLine = loc.range.end.line + 1;
        int startChar = loc.range.start.character;
        int endChar = loc.range.end.character;
        
        report += "📍 Definition #" + std::to_string(i + 1) + ":\n";
        report += "   └─ File: `" + locPath + "`\n";
        if (startLine == endLine) {
            report += "   └─ Line: " + std::to_string(startLine) + 
                      " | Characters: " + std::to_string(startChar) + "-" + std::to_string(endChar) + "\n";
        } else {
            report += "   └─ Lines: " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                      " | Start: " + std::to_string(startChar) + " | End: " + std::to_string(endChar) + "\n";
        }
        std::string relPath = locPath;
        if (relPath.find(rootPath.u8string()) == 0) {
            relPath = fs::relative(fs::u8path(locPath), rootPath).generic_string();
        }
        int contextStart = std::max(1, startLine - 10);
        int contextEnd = endLine + 10;
        report += "   └─ Read context: read({mode:{type:\"range\",start:" +
                  std::to_string(contextStart) + ", end:" +
                  std::to_string(contextEnd) + "}, path:\"" + relPath +
                  "\", reason:\"inspect range\"})\n\n";
    }
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::lspReferences(const nlohmann::json& args) {
    auto pickClient = [&](const std::string& relPath) -> LSPClient* {
        if (!lspByExtension.empty()) {
            std::string ext = fs::path(relPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            auto it = lspByExtension.find(ext);
            if (it != lspByExtension.end()) return it->second;
        }
        return lspClient;
    };

    std::string relPath = args["path"];
    int line = args["line"];
    int character = args["character"];
    
    LSPClient* client = pickClient(relPath);
    if (!client) {
        return {{"error", "LSP client not initialized for this file type."}};
    }

    fs::path fullPath = fs::absolute(rootPath / fs::u8path(relPath));
    std::string fileUri = "file://" + fullPath.u8string();

    LSPClient::Position position{line - 1, character};
    auto locations = client->findReferences(fileUri, position);

    if (locations.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "No references found by LSP."}}}}};
    }

    std::string report = "Found " + std::to_string(locations.size()) + " reference(s):\n\n";
    for (size_t i = 0; i < locations.size(); ++i) {
        const auto& loc = locations[i];
        std::string locPath = loc.uri;
        if (locPath.find("file://") == 0) locPath = locPath.substr(7);
        int startLine = loc.range.start.line + 1;
        int endLine = loc.range.end.line + 1;
        
        std::string displayPath = locPath;
        if (displayPath.find(rootPath.u8string()) == 0) {
            displayPath = fs::relative(fs::u8path(locPath), rootPath).generic_string();
        }

        report += "📍 Reference #" + std::to_string(i + 1) + ":\n";
        report += "   └─ File: `" + displayPath + "` | Line: " + std::to_string(startLine) + "\n";
        
        int contextStart = std::max(1, startLine - 2);
        int contextEnd = endLine + 2;
        report += "   └─ Quick view: read({mode:{type:\"range\",start:" +
                  std::to_string(contextStart) + ", end:" +
                  std::to_string(contextEnd) + "}, path:\"" + displayPath +
                  "\", reason:\"inspect range\"})\n\n";
    }
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::generateLogicMap(const nlohmann::json& args) {
    if (!symbolManager) return {{"error", "Symbol manager not initialized"}};
    
    std::string entrySymbol = args["entry_symbol"];
    int maxDepth = args.value("max_depth", 3);
    
    // Check cache
    fs::path cachePath = globalDataPath / "index" / "logic_map.json";
    nlohmann::json cache;
    if (fs::exists(cachePath)) {
        std::ifstream f(cachePath);
        try {
            f >> cache;
            if (cache.contains(entrySymbol) && cache[entrySymbol].value("max_depth", 0) >= maxDepth) {
                // Check if any files have changed since cache was created
                // For simplicity, we'll just return the cache for now.
                // In a full implementation, we'd check mtimes.
                return {{"content", {{{"type", "text"}, {"text", cache[entrySymbol]["map"].dump(2)}}}}};
            }
        } catch (...) {}
    }

    LogicMapper mapper(*symbolManager, lspByExtension, lspClient);
    nlohmann::json map = mapper.generateMap(entrySymbol, maxDepth);
    
    // Save to cache
    try {
        fs::create_directories(cachePath.parent_path());
        cache[entrySymbol] = {{"max_depth", maxDepth}, {"map", map}};
        std::ofstream f(cachePath);
        f << cache.dump(2);
    } catch (...) {}

    return {{"content", {{{"type", "text"}, {"text", map.dump(2)}}}}};
}

nlohmann::json InternalMCPClient::resolveRelativeDate(const nlohmann::json& args) {
    std::string fuzzyDateStr = args["fuzzy_date"];
    std::transform(fuzzyDateStr.begin(), fuzzyDateStr.end(), fuzzyDateStr.begin(), ::tolower);
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &now_t);
#else
    localtime_r(&now_t, &timeinfo);
#endif
    auto formatDate = [](std::tm* tm_ptr) {
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm_ptr);
        return std::string(buf);
    };
    if (fuzzyDateStr == "today" || fuzzyDateStr == "今天") return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    if (fuzzyDateStr == "yesterday" || fuzzyDateStr == "昨天") {
        timeinfo.tm_mday -= 1; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    if (fuzzyDateStr == "tomorrow" || fuzzyDateStr == "明天") {
        timeinfo.tm_mday += 1; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    if (fuzzyDateStr == "last week" || fuzzyDateStr == "上周") {
        timeinfo.tm_mday -= 7; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    if (fuzzyDateStr == "last month" || fuzzyDateStr == "上个月") {
        timeinfo.tm_mon -= 1; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    if (fuzzyDateStr == "last year" || fuzzyDateStr == "去年") {
        timeinfo.tm_year -= 1; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    static const std::regex daysAgoRegex("(\\d+)\\s+(?:days?|天)\\s+(?:ago|前)");
    std::smatch match;
    if (std::regex_search(fuzzyDateStr, match, daysAgoRegex)) {
        int days = std::stoi(match[1].str());
        timeinfo.tm_mday -= days; std::mktime(&timeinfo);
        return {{"content", {{{"type", "text"}, {"text", formatDate(&timeinfo)}}}}};
    }
    return {{"error", "Could not resolve relative date: " + fuzzyDateStr}};
}

nlohmann::json InternalMCPClient::skillRead(const nlohmann::json& args) {
    if (!skillManager) {
        return {{"error", "Skill Manager not initialized"}};
    }
    std::string name = args["name"];
    std::string content = skillManager->getSkillContent(name);
    return {{"content", {{{"type", "text"}, {"text", content}}}}};
}

nlohmann::json InternalMCPClient::osScheduler(const nlohmann::json& args) {
    // Proactively clean up dead tasks to prevent memory growth
    listTasks({});

    int delay = args["delay_seconds"];
    std::string type = args["type"];
    std::string payload = args["payload"];
    bool isPeriodic = args.value("is_periodic", false);
    std::string taskId = args.value("task_id", "");
    
    if (delay < 0) return {{"error", "Delay cannot be negative"}};

    // If task_id is provided, check for duplicates and cancel them
    if (!taskId.empty()) {
        auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const BackgroundTask& t) {
            if (t.id == taskId) {
                killTaskProcess(t.pid);
                return true;
            }
            return false;
        });
        tasks.erase(it, tasks.end());
    } else {
        taskId = generateTaskId();
    }

    std::string cmd;
    std::string logFileName = "task_" + taskId + ".log";
    fs::path logPath = globalDataPath / "logs" / logFileName;
    fs::create_directories(globalDataPath / "logs");
    
    // Construct the command content
    std::string coreCmd;
    if (type == "notify") {
        // ... (existing notify logic)
#ifdef _WIN32
        // Escape single quotes for PowerShell
        std::string safePayload;
        for (char c : payload) {
            if (c == '\'') safePayload += "''";
            else safePayload += c;
        }
        coreCmd = "powershell -Command \"[reflection.assembly]::loadwithpartialname('System.Windows.Forms'); [System.Windows.Forms.MessageBox]::Show('" + safePayload + "', 'Photon Scheduler')\"";
#else
        std::string safePayload;
        for (char c : payload) {
            if (c == '"') safePayload += "\\\"";
            else if (c == '\\') safePayload += "\\\\";
            else if (c == '\'') safePayload += "\\\'"; // Add single quote escape
            else safePayload += c;
        }
        // Simplified macOS notification command with a sound hint for better debugging
        coreCmd = "osascript -e 'display notification \"" + safePayload + "\" with title \"Photon Reminder\"' && afplay /System/Library/Sounds/Tink.aiff";
#endif
    } else {
        if (!isCommandSafe(payload)) {
            return {{"error", "Security Alert: Scheduled command blocked by Photon Guard."}};
        }
        coreCmd = payload;
    }

    // Wrap in periodicity logic and redirection (No auto-retry, focus on log capture for Agent analysis)
    if (isPeriodic) {
#ifdef _WIN32
        // Windows loop with redirection
        cmd = "powershell -Command \"(Start-Process powershell -ArgumentList '-NoProfile', '-Command', 'while($true){ $start=[DateTime]::Now; " + coreCmd + " >> ''" + logPath.u8string() + "'' 2>&1; $wait=" + std::to_string(delay) + "-([DateTime]::Now-$start).TotalSeconds; if($wait -gt 0){ Start-Sleep -Seconds $wait } }' -WindowStyle Hidden -PassThru).Id\"";
#else
        // Linux/macOS loop with redirection
        cmd = "nohup sh -c 'while true; do NEXT=$(( $(date +%s) + " + std::to_string(delay) + " )); { " + coreCmd + "; } >> \"" + logPath.u8string() + "\" 2>&1; NOW=$(date +%s); DIFF=$(( NEXT - NOW )); [ $DIFF -gt 0 ] && sleep $DIFF; done' >/dev/null 2>&1 & echo $!";
#endif
    } else {
#ifdef _WIN32
        // One-time task for Windows
        cmd = "powershell -Command \"(Start-Process cmd -ArgumentList '/c timeout /t " + std::to_string(delay) + " /nobreak >nul && " + coreCmd + " >> ''" + logPath.u8string() + "'' 2>&1' -WindowStyle Hidden -PassThru).Id\"";
#else
        // One-time task for POSIX
        cmd = "nohup sh -c 'sleep " + std::to_string(delay) + " && { " + coreCmd + "; } >> \"" + logPath.u8string() + "\" 2>&1' >/dev/null 2>&1 & echo $!";
#endif
    }

    // Execute and capture PID
    std::string output = executeCommand(cmd);
    int pid = 0;
    try {
        pid = std::stoi(output);
    } catch (...) {}

    tasks.push_back({taskId, (isPeriodic ? "[Periodic] " : "[One-time] ") + type + ": " + payload, pid, isPeriodic, delay, std::time(nullptr), logPath.u8string()});
    
    saveTasksToDisk(); // Persist tasks to disk

    // Auto-save to long-term memory for persistence
    toolMemory({{"action", "write"}, {"key", "task_log_" + taskId},
        {"value", "Scheduled: " + type + " - " + payload + " (Logs: " + logPath.u8string() + ")"}, {"type", "fact"}});

    return {{"content", {{{"type", "text"}, {"text", "Task scheduled: " + taskId + " (PID: " + std::to_string(pid) + "). Logs redirected to: " + logPath.u8string()}}}}};
}

nlohmann::json InternalMCPClient::listTasks(const nlohmann::json& args) {
    // Clean up dead tasks before listing
    // Since we don't have a background polling thread, we do a lazy cleanup here.
    // Check if PID is still running.
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const BackgroundTask& t) {
        // For all tasks, check if process is still alive.
        if (t.pid <= 0) return true; // Invalid PID
        
#ifdef _WIN32
        // On Windows, tasklist is a common way to check if PID is alive
        std::string checkCmd = "tasklist /FI \"PID eq " + std::to_string(t.pid) + "\" 2>nul";
        std::string output = executeCommand(checkCmd);
        if (output.find(std::to_string(t.pid)) == std::string::npos) {
            return true; // Process is gone
        }
#else
        // On POSIX, kill(pid, 0) checks existence.
        if (kill(t.pid, 0) != 0) {
            return true; // Process is gone, remove from list
        }
#endif
        return false;
    });
    tasks.erase(it, tasks.end());
    saveTasksToDisk(); // Update persistence

    if (tasks.empty()) return {{"content", {{{"type", "text"}, {"text", "No active background tasks."}}}}};
    
    std::string report = "Active Tasks:\n";
    for (const auto& t : tasks) {
        report += "- ID: " + t.id + " | PID: " + std::to_string(t.pid) + " | " + t.description;
        if (!t.logPath.empty()) {
            report += " | Logs: " + t.logPath;
            // Check if log file has content (indicating potential errors or output)
            if (fs::exists(fs::u8path(t.logPath))) {
                auto size = fs::file_size(fs::u8path(t.logPath));
                if (size > 0) {
                    report += " (" + std::to_string(size) + " bytes)";
                }
            }
        }
        report += "\n";
    }
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::cancelTask(const nlohmann::json& args) {
    std::string targetId = args["task_id"];
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const BackgroundTask& t) {
        if (t.id == targetId) {
            killTaskProcess(t.pid);
            return true;
        }
        return false;
    });

    if (it != tasks.end()) {
        tasks.erase(it, tasks.end());
        saveTasksToDisk(); // Update persistence
        return {{"content", {{{"type", "text"}, {"text", "Task cancelled: " + targetId}}}}};
    }
    return {{"error", "Task ID not found: " + targetId}};
}

nlohmann::json InternalMCPClient::readTaskLog(const nlohmann::json& args) {
    std::string targetId = args["task_id"];
    int lines = args.value("lines", 50); // Default to last 50 lines

    for (const auto& t : tasks) {
        if (t.id == targetId) {
            if (t.logPath.empty() || !fs::exists(fs::u8path(t.logPath))) {
                return {{"error", "Log file not found for task: " + targetId}};
            }

            std::ifstream file(fs::u8path(t.logPath));
            std::vector<std::string> logLines;
            std::string line;
            while (std::getline(file, line)) {
                logLines.push_back(line);
            }

            // Get the last N lines
            std::string result;
            int start = std::max(0, static_cast<int>(logLines.size()) - lines);
            for (size_t i = start; i < logLines.size(); ++i) {
                result += logLines[i] + "\n";
            }

            if (result.empty()) {
                return {{"content", {{{"type", "text"}, {"text", "(Log file is empty)"}}}}};
            }

            return {{"content", {{{"type", "text"}, {"text", "Last " + std::to_string(logLines.size() - start) + " lines of log for " + targetId + ":\n\n" + result}}}}};
        }
    }
    return {{"error", "Task ID not found: " + targetId}};
}

nlohmann::json InternalMCPClient::toolFileRead(const nlohmann::json& args) {
    auto hasReadArgs = [&](const nlohmann::json& a) {
        return a.contains("mode") || a.contains("path") || a.contains("query") ||
               a.contains("start_line") || a.contains("end_line") || a.contains("requests") ||
               a.contains("center_line");
    };
    if (enforceContextPlan && plannedReads.empty() && hasReadArgs(args)) {
        return {{"content", {{{"type", "text"},
            {"text", "⚠️  请先使用 plan 生成检索计划，再执行 read。"}}}}};
    }

    if (args.contains("requests")) {
        return readBatchLines(args);
    }

    std::string reason = args.value("reason", "");
    std::string path = args.value("path", "");
    if (path.empty() && !lastReadTarget.empty()) {
        path = lastReadTarget;
    }

    nlohmann::json mode;
    if (args.contains("mode") && args["mode"].is_object()) {
        mode = args["mode"];
    } else if (args.contains("query")) {
        mode = {{"type", "symbol"}, {"name", args["query"]}};
    } else if (args.contains("start_line") || args.contains("end_line") || args.contains("center_line")) {
        mode = {{"type", "range"}};
    } else if (!path.empty()) {
        mode = {{"type", "full"}};
    }

    if (!mode.contains("type")) {
        return {{"content", {{{"type", "text"}, {"text",
            "❌ READ_FAIL: missing mode. Provide mode: full | range | symbol."}}}}};
    }

    std::string modeType = mode.value("type", "");
    if (reason.empty()) {
        std::string hint = "❌ READ_FAIL: reason is required for read.";
        if (!path.empty()) hint += " You can reuse path: " + path;
        return {{"content", {{{"type", "text"}, {"text", hint}}}}};
    }

    if (modeType == "symbol") {
        if (!path.empty()) lastReadTarget = path;
        nlohmann::json symArgs = {
            {"symbol", mode.value("name", args.value("symbol", args.value("query", "")))},
            {"path", path},
            {"context_lines", args.value("context_lines", 40)},
            {"reason", reason}
        };
        return readSymbol(symArgs);
    }

    if (modeType == "range") {
        int start = mode.value("start", args.value("start_line", 0));
        int end = mode.value("end", args.value("end_line", 0));
        if ((start <= 0 || end <= 0) && args.contains("center_line")) {
            int center = args.value("center_line", 0);
            int before = args.value("before", 20);
            int after = args.value("after", 20);
            if (before > 30) before = 30;
            if (after > 30) after = 30;
            start = center - before;
            end = center + after;
        }
        if (start < 1) start = 1;
        if (end < start) end = start;

        if (enforceContextPlan && !plannedReads.empty() && !path.empty()) {
            const PlannedRead* match = nullptr;
            for (const auto& pr : plannedReads) {
                if (pr.path != path) continue;
                if (end >= pr.startLine && start <= pr.endLine) {
                    match = &pr;
                    break;
                }
            }
            if (!match) {
                std::string hint = "⚠️  读取范围不在检索计划内。请先更新 plan。\n";
                hint += "可用范围示例：\n";
                int shown = 0;
                for (const auto& pr : plannedReads) {
                    if (shown++ >= 3) break;
                    hint += "- " + pr.path + " " + std::to_string(pr.startLine) + "-" + std::to_string(pr.endLine) + " (" + pr.label + ")\n";
                }
                return {{"content", {{{"type", "text"}, {"text", hint}}}}};
            }
            if (start < match->startLine || end > match->endLine) {
                start = match->startLine;
                end = match->endLine;
            }
        }

        if (path.empty()) {
            return {{"content", {{{"type", "text"}, {"text",
                "❌ READ_FAIL: path is required for range mode."}}}}};
        }
        lastReadTarget = path;
        nlohmann::json rangeArgs = {
            {"path", path},
            {"start_line", start},
            {"end_line", end},
            {"reason", reason}
        };
        return readFileRange(rangeArgs);
    }

    if (modeType == "full") {
        if (path.empty()) {
            return {{"content", {{{"type", "text"}, {"text",
                "❌ READ_FAIL: path is required for full mode."}}}}};
        }
        lastReadTarget = path;
        nlohmann::json fullArgs = {{"path", path}, {"reason", reason}};
        return readFullFile(fullArgs);
    }

    return {{"content", {{{"type", "text"}, {"text", "❌ READ_FAIL: unknown mode type."}}}}};
}

nlohmann::json InternalMCPClient::toolFileWrite(const nlohmann::json& args) {
    nlohmann::json normalized = args;
    std::string reason = normalized.value("reason", "");

    auto hasWriteIntent = [&](const nlohmann::json& a) {
        return a.contains("mode") || a.contains("operation") || a.contains("search") ||
               a.contains("replace") || a.contains("edits") || a.contains("content");
    };
    if (hasWriteIntent(normalized) && reason.empty()) {
        return {{"content", {{{"type", "text"}, {"text",
            "❌ WRITE_FAIL: reason is required for write."}}}}};
    }

    auto fillPath = [&](nlohmann::json& a) {
        if (!a.contains("path") || a["path"].get<std::string>().empty()) {
            if (!lastReadTarget.empty()) {
                a["path"] = lastReadTarget;
            }
        }
    };
    auto resolveSymbolRange = [&](const std::string& symbol,
                                  const std::string& pathFilter,
                                  int& outStart,
                                  int& outEnd) -> bool {
        if (!symbolManager) return false;
        auto results = symbolManager->search(symbol);
        if (!pathFilter.empty()) {
            std::vector<SymbolManager::Symbol> filtered;
            for (const auto& s : results) {
                if (s.path == pathFilter) filtered.push_back(s);
            }
            results.swap(filtered);
        }
        if (results.empty()) return false;
        const auto* picked = &results.front();
        for (const auto& s : results) {
            if (s.name == symbol) { picked = &s; break; }
        }
        outStart = picked->line;
        outEnd = (picked->endLine > 0 ? picked->endLine : picked->line);
        if (outStart < 1) outStart = 1;
        if (outEnd < outStart) outEnd = outStart;
        if (normalized.contains("path") == false || normalized["path"].get<std::string>().empty()) {
            normalized["path"] = picked->path;
        }
        return true;
    };

    if (normalized.contains("mode") && normalized["mode"].is_object()) {
        auto mode = normalized["mode"];
        std::string type = mode.value("type", "");
        if (type == "bulk") {
            if (mode.contains("edits")) {
                normalized["edits"] = mode["edits"];
            } else if (!normalized.contains("edits")) {
                return {{"content", {{{"type", "text"}, {"text",
                    "❌ WRITE_FAIL: bulk mode requires edits."}}}}};
            }
        } else if (type == "diff") {
            fillPath(normalized);
            if (mode.contains("search")) normalized["search"] = mode["search"];
            if (mode.contains("replace")) normalized["replace"] = mode["replace"];
        } else if (type == "insert" || type == "replace" || type == "delete") {
            fillPath(normalized);
            normalized["operation"] = type;
            if (mode.contains("start")) normalized["start_line"] = mode["start"];
            if (mode.contains("end")) normalized["end_line"] = mode["end"];
            if (mode.contains("content")) normalized["content"] = mode["content"];
            if (mode.contains("symbol") && (!normalized.contains("start_line") || !normalized.contains("end_line"))) {
                int start = 0;
                int end = 0;
                if (!resolveSymbolRange(mode.value("symbol", ""), normalized.value("path", ""), start, end)) {
                    return {{"content", {{{"type", "text"}, {"text",
                        "❌ WRITE_FAIL: symbol not found for write."}}}}};
                }
                if (!normalized.contains("start_line")) normalized["start_line"] = start;
                if (!normalized.contains("end_line")) normalized["end_line"] = end;
            }
        }
    } else {
        fillPath(normalized);
        if (normalized.contains("symbol") && normalized.contains("operation") &&
            (!normalized.contains("start_line") || !normalized.contains("end_line"))) {
            int start = 0;
            int end = 0;
            if (!resolveSymbolRange(normalized.value("symbol", ""), normalized.value("path", ""), start, end)) {
                return {{"content", {{{"type", "text"}, {"text",
                    "❌ WRITE_FAIL: symbol not found for write."}}}}};
            }
            if (!normalized.contains("start_line")) normalized["start_line"] = start;
            if (!normalized.contains("end_line")) normalized["end_line"] = end;
        }
    }
    if (!normalized.contains("edits") && (!normalized.contains("path") || normalized["path"].get<std::string>().empty())) {
        return {{"content", {{{"type", "text"}, {"text",
            "❌ WRITE_FAIL: path is required for non-bulk write."}}}}};
    }

    bool highRisk = false;
    std::string targetPath;
    if (normalized.contains("path")) {
        targetPath = normalized["path"];
        highRisk = isHighRiskPath(targetPath);
    } else if (normalized.contains("edits")) {
        for (const auto& edit : normalized["edits"]) {
            if (edit.contains("path") && isHighRiskPath(edit["path"])) {
                highRisk = true;
                targetPath = edit["path"];
                break;
            }
        }
    }

    if (highRisk && !sessionAuthorized) {
        return {
            {"status", "requires_confirmation"},
            {"is_high_risk", true},
            {"content", {{{"type", "text"}, {"text", "⚠️ 安全拦截：修改核心源码（src/）需要您的显式授权。请告知用户需要点击授权按钮或调用 'authorize' 工具来开启本次对话的执行权限。"}}}}
        };
    }

    auto isNewFileWrite = [&](const nlohmann::json& a) {
        if (!a.contains("path") || !a.contains("content")) return false;
        if (a.contains("operation")) return false;
        if (a.contains("search") && a.contains("replace")) return false;
        std::string path = a["path"];
        fs::path full = rootPath / fs::u8path(path);
        return !fs::exists(full);
    };
    auto requiresPriorRead = [&](const std::string& relPath) {
        fs::path full = rootPath / fs::u8path(relPath);
        if (!fs::exists(full)) return false;
        return readPaths.find(relPath) == readPaths.end();
    };
    auto isFullOverwrite = [&](const nlohmann::json& a) {
        if (!a.contains("path") || !a.contains("content")) return false;
        if (a.contains("operation")) return false;
        if (a.contains("search") && a.contains("replace")) return false;
        return true;
    };

    if (normalized.contains("path")) {
        std::string path = normalized["path"];
        if (requiresPriorRead(path)) {
            return {{"content", {{{"type", "text"},
                {"text", "⚠️ 写入前请先 read 该文件并完成摘要，再进行修改：`" + path + "`"}}}}};
        }
        if (isFullOverwrite(normalized)) {
            fs::path full = rootPath / fs::u8path(path);
            if (fs::exists(full)) {
                return {{"content", {{{"type", "text"},
                    {"text", "⚠️ 禁止对已存在文件进行整文件覆写。请使用 operation 或 search/replace。"}}}}};
            }
        }
    } else if (normalized.contains("edits")) {
        for (const auto& edit : normalized["edits"]) {
            if (!edit.contains("path")) continue;
            std::string path = edit["path"];
            if (requiresPriorRead(path)) {
                return {{"content", {{{"type", "text"},
                    {"text", "⚠️ 写入前请先 read 该文件并完成摘要，再进行修改：`" + path + "`"}}}}};
            }
        }
    }

    // Level 2 Logging: Log normal file writes
    if (!targetPath.empty()) {
        Logger::getInstance().action("Writing to file: " + targetPath);
    }

    if (normalized.contains("edits")) {
        return editBatchLines(normalized);
    }
    if (normalized.contains("operation")) {
        return fileEditLines(normalized);
    }
    if (normalized.contains("search") && normalized.contains("replace")) {
        return diffApply(normalized);
    }
    return fileWrite(normalized);
}

nlohmann::json InternalMCPClient::toolSearch(const nlohmann::json& args) {
    std::string query = args.value("query", "");
    if (query.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ SEARCH_FAIL: query is required."}}}}};
    }
    std::string type = args.value("type", "");
    std::string path = args.value("path", "");
    int maxResults = args.value("max_results", 5);
    if (maxResults < 1) maxResults = 1;

    auto runSymbol = [&]() -> nlohmann::json {
        if (!symbolManager) {
            return {{"content", {{{"type", "text"}, {"text", "❌ SEARCH_FAIL: SymbolManager not initialized."}}}}};
        }
        auto results = symbolManager->search(query);
        if (!path.empty()) {
            std::vector<SymbolManager::Symbol> filtered;
            for (const auto& s : results) {
                if (s.path == path) filtered.push_back(s);
            }
            results.swap(filtered);
        }
        if (results.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "No symbols found matching: " + query}}}}};
        }
        if ((int)results.size() > maxResults) results.resize(maxResults);
        std::string report = "Found " + std::to_string(results.size()) + " matches:\n";
        for (const auto& s : results) {
            std::string source = s.source.empty() ? "unknown" : s.source;
            report += "📍 [" + s.type + "|" + source + "] `" + s.name + "`\n";
            report += "   └─ File: `" + s.path + "` | Line: " + std::to_string(s.line) + "\n";
        }
        return {{"content", {{{"type", "text"}, {"text", report}}}}};
    };

    auto runText = [&]() -> nlohmann::json {
        std::string results;
        if (isGitRepo) {
            std::string cmd = "git -C " + rootPath.u8string() + " grep -n -i -I --untracked --context=1 \"" + query + "\"";
            results = executeCommand(cmd);
            if (!results.empty()) {
                return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 8000)}}}}};
            }
        }
        std::vector<fs::path> targetFiles;
        try {
            auto iter = fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied);
            for (const auto& entry : iter) {
                if (shouldIgnore(entry.path())) {
                    if (entry.is_directory()) iter.disable_recursion_pending();
                    continue;
                }
                if (entry.is_regular_file()) {
                    targetFiles.push_back(entry.path());
                }
            }
        } catch (...) {}

        std::string lowerPattern = query;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

        std::mutex outputMutex;
        std::vector<std::future<void>> futures;
        std::string output;
        for (const auto& file : targetFiles) {
            futures.push_back(std::async(std::launch::async, [&]() {
                std::ifstream in(file);
                if (!in.is_open()) return;
                std::string line;
                int lineNum = 0;
                std::string fileOut;
                while (std::getline(in, line)) {
                    lineNum++;
                    std::string lowerLine = line;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                    if (lowerLine.find(lowerPattern) != std::string::npos) {
                        std::string rel = fs::relative(file, rootPath).generic_string();
                        fileOut += rel + ":" + std::to_string(lineNum) + ":" + line + "\n";
                        if (fileOut.size() > 2000) break;
                    }
                }
                if (!fileOut.empty()) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    output += fileOut;
                }
            }));
        }
        for (auto& f : futures) f.wait();
        if (output.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "No results found for: " + query}}}}};
        }
        return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(output, 8000)}}}}};
    };

    auto runFile = [&]() -> nlohmann::json {
        return fileSearch({{"query", query}});
    };

    auto runSemantic = [&]() -> nlohmann::json {
        if (!semanticManager) {
            return {{"content", {{{"type", "text"}, {"text", "SemanticManager not initialized."}}}}};
        }
        auto results = semanticManager->search(query, maxResults);
        if (results.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "No semantic matches found for: " + query}}}}};
        }
        std::string report = "Semantic Search Results for: `" + query + "`\n\n";
        for (const auto& chunk : results) {
            std::string typeIcon = "🧩";
            if (chunk.type == "code") typeIcon = "⚙️";
            else if (chunk.type == "markdown") typeIcon = "📄";
            else if (chunk.type == "fact") typeIcon = "💡";
            else if (chunk.type == "skill") typeIcon = "📜";
            report += "📍 " + typeIcon + " **[" + chunk.type + "]** in `" + chunk.path + "`";
            if (chunk.startLine > 0) {
                report += " (Line " + std::to_string(chunk.startLine) + ")";
            }
            report += " | Score: " + std::to_string(static_cast<int>(chunk.score * 100)) + "%\n";
            std::string preview = chunk.content;
            if (preview.length() > 300) preview = preview.substr(0, 300) + "...";
            report += "   > " + preview + "\n\n";
        }
        report += "*Tip: Use 'read' with mode range/symbol to examine full context of these matches.*";
        return {{"content", {{{"type", "text"}, {"text", report}}}}};
    };

    if (type.empty()) {
        if (query.find('/') != std::string::npos || query.find('.') != std::string::npos) {
            type = "file";
        } else {
            type = "symbol";
        }
    }

    if (type == "symbol") {
        auto res = runSymbol();
        if (res.contains("content") && res["content"].is_array() && !res["content"].empty()) {
            auto& item = res["content"][0];
            if (item.contains("text")) {
                std::string text = item["text"].get<std::string>();
                if (text.find("No symbols found matching") != std::string::npos) {
                    return runText();
                }
            }
        }
        return res;
    }
    if (type == "text") return runText();
    if (type == "file") return runFile();
    if (type == "semantic") return runSemantic();

    return {{"content", {{{"type", "text"}, {"text", "❌ SEARCH_FAIL: unknown type."}}}}};
}

nlohmann::json InternalMCPClient::toolPlan(const nlohmann::json& args) {
    if (!args.contains("query") || args["query"].get<std::string>().empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ PLAN_FAIL: query is required."}}}}};
    }
    nlohmann::json planArgs = args;
    if (args.contains("options") && args["options"].is_object()) {
        auto opts = args["options"];
        if (opts.contains("max_steps") && !planArgs.contains("max_entries")) {
            planArgs["max_entries"] = opts["max_steps"];
        }
        if (opts.contains("budget")) {
            planArgs["budget"] = opts["budget"];
        }
    }
    return contextPlan(planArgs);
}

nlohmann::json InternalMCPClient::toolReason(const nlohmann::json& args) {
    std::string type = args.value("type", "");
    if (type.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: type is required."}}}}};
    }
    std::string target = args.value("target", "");
    if (args.contains("targets") && args["targets"].is_array() && !args["targets"].empty()) {
        target = args["targets"][0].get<std::string>();
    }
    std::string path = args.value("path", "");

    if (type == "call_graph") {
        if (target.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: target is required for call_graph."}}}}};
        }
        int maxDepth = args.value("max_depth", 3);
        return generateLogicMap({{"entry_symbol", target}, {"max_depth", maxDepth}});
    }

    if (type == "dependency") {
        if (path.empty()) {
            if (!target.empty() && symbolManager) {
                auto results = symbolManager->search(target);
                if (!results.empty()) {
                    path = results.front().path;
                }
            }
        }
        if (path.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: path or resolvable target is required for dependency."}}}}};
        }
        return codeAstAnalyze({{"path", path}});
    }

    if (type == "logic_check") {
        if (target.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: target is required for logic_check."}}}}};
        }
        int maxDepth = args.value("max_depth", 2);
        return generateLogicMap({{"entry_symbol", target}, {"max_depth", maxDepth}});
    }

    if (type == "summary_inference") {
        return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: summary_inference requires model-side summarization; use read summaries instead."}}}}};
    }

    return {{"content", {{{"type", "text"}, {"text", "❌ REASON_FAIL: unknown type."}}}}};
}

nlohmann::json InternalMCPClient::toolHistory(const nlohmann::json& args) {
    std::string action = args.value("action", "query");
    if (action == "append") {
        if (!args.contains("operation") || !args["operation"].is_object()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ HISTORY_FAIL: operation is required for append."}}}}};
        }
        nlohmann::json entry = args["operation"];
        entry["timestamp"] = static_cast<long long>(std::time(nullptr));
        entry["snapshot"] = {
            {"last_read", lastReadTarget},
            {"read_paths", static_cast<int>(readPaths.size())},
            {"planned_reads", static_cast<int>(plannedReads.size())},
            {"last_plan_query", lastPlanQuery}
        };
        historyEntries.push_back(entry);
        if (historyEntries.size() > maxHistoryEntries) {
            historyEntries.erase(historyEntries.begin());
        }
        return {{"content", {{{"type", "text"}, {"text", "✅ HISTORY_OK: appended."}}}}};
    }

    auto matchesFilter = [&](const nlohmann::json& entry, const nlohmann::json& filter) {
        if (!filter.is_object()) return true;
        if (filter.contains("tool")) {
            std::string tool = filter.value("tool", "");
            if (!tool.empty() && entry.value("tool", "") != tool) return false;
        }
        if (filter.contains("path")) {
            std::string path = filter.value("path", "");
            if (!path.empty()) {
                std::string dump = entry.contains("args") ? entry["args"].dump() : entry.dump();
                if (dump.find(path) == std::string::npos) return false;
            }
        }
        if (filter.contains("contains")) {
            std::string needle = filter.value("contains", "");
            if (!needle.empty()) {
                std::string dump = entry.dump();
                if (dump.find(needle) == std::string::npos) return false;
            }
        }
        return true;
    };

    if (action == "query") {
        nlohmann::json filter = args.value("filter", nlohmann::json::object());
        int limit = filter.value("limit", 20);
        if (limit < 1) limit = 1;
        nlohmann::json out = nlohmann::json::array();
        for (auto it = historyEntries.rbegin(); it != historyEntries.rend(); ++it) {
            if (!matchesFilter(*it, filter)) continue;
            out.push_back(*it);
            if ((int)out.size() >= limit) break;
        }
        return {{"content", {{{"type", "text"}, {"text", out.dump(2)}}}}};
    }

    if (action == "remove") {
        nlohmann::json filter = args.value("filter", nlohmann::json::object());
        if (filter.empty()) {
            size_t count = historyEntries.size();
            historyEntries.clear();
            return {{"content", {{{"type", "text"}, {"text", "✅ HISTORY_OK: cleared " + std::to_string(count) + " entries."}}}}};
        }
        size_t before = historyEntries.size();
        historyEntries.erase(std::remove_if(historyEntries.begin(), historyEntries.end(),
            [&](const nlohmann::json& e) { return matchesFilter(e, filter); }),
            historyEntries.end());
        size_t removed = before - historyEntries.size();
        return {{"content", {{{"type", "text"}, {"text", "✅ HISTORY_OK: removed " + std::to_string(removed) + " entries."}}}}};
    }

    return {{"content", {{{"type", "text"}, {"text", "❌ HISTORY_FAIL: unknown action."}}}}};
}

nlohmann::json InternalMCPClient::toolMemory(const nlohmann::json& args) {
    std::string action = args.value("action", "");
    if (action.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "❌ MEMORY_FAIL: action is required."}}}}};
    }

    fs::path memoryPath = globalDataPath / "memory.json";
    nlohmann::json memory = nlohmann::json::object();
    if (fs::exists(memoryPath)) {
        std::ifstream f(memoryPath);
        try { f >> memory; } catch (...) {}
    }
    if (!memory.contains("facts")) memory["facts"] = nlohmann::json::object();
    if (!memory.contains("knowledge_index")) memory["knowledge_index"] = nlohmann::json::array();

    if (action == "write") {
        std::string key = args.value("key", "");
        std::string value = args.value("value", "");
        std::string type = args.value("type", "fact");
        if (key.empty() || value.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ MEMORY_FAIL: key and value are required for write."}}}}};
        }
        if (type == "knowledge") {
            fs::path kbPath = globalDataPath / "knowledge.md";
            std::ofstream f(kbPath, std::ios::app);
            if (!f.is_open()) return {{"error", "Could not open knowledge.md for writing"}};
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            struct tm time_info;
#ifdef _WIN32
            localtime_s(&time_info, &in_time_t);
#else
            localtime_r(&in_time_t, &time_info);
#endif
            ss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
            f << "\n## " << key << " (" << ss.str() << ")\n" << value << "\n";
            f.close();
            memory["knowledge_index"].push_back({{"title", key}, {"timestamp", ss.str()}});
        } else {
            memory["facts"][key] = value;
        }
        std::ofstream fo(memoryPath);
        if (!fo.is_open()) return {{"error", "Could not open memory.json for writing"}};
        fo << memory.dump(4);
        return {{"content", {{{"type", "text"}, {"text", "✅ MEMORY_OK: stored."}}}}};
    }

    if (action == "read") {
        std::string key = args.value("key", "");
        if (key.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ MEMORY_FAIL: key is required for read."}}}}};
        }
        if (memory["facts"].contains(key)) {
            return {{"content", {{{"type", "text"}, {"text", memory["facts"][key].get<std::string>()}}}}};
        }
        if (key == "knowledge.md" || key == "knowledge") {
            fs::path kbPath = globalDataPath / "knowledge.md";
            if (fs::exists(kbPath)) {
                std::ifstream kbf(kbPath);
                std::string content((std::istreambuf_iterator<char>(kbf)), std::istreambuf_iterator<char>());
                return {{"content", {{{"type", "text"}, {"text", content}}}}};
            }
        }
        return {{"content", {{{"type", "text"}, {"text", "Key not found in memory."}}}}};
    }

    if (action == "query") {
        std::string report = "Photon Long-term Memory Index:\n\n";
        if (!memory["facts"].empty()) {
            report += "### [Facts]\n";
            for (auto& [key, val] : memory["facts"].items()) {
                report += "- " + key + "\n";
            }
            report += "\n";
        }
        if (!memory["knowledge_index"].empty()) {
            report += "### [Knowledge]\n";
            for (const auto& item : memory["knowledge_index"]) {
                report += "- " + item.value("title", "Untitled") + " (" + item.value("timestamp", "") + ")\n";
            }
        }
        if (memory["facts"].empty() && memory["knowledge_index"].empty()) {
            report += "(No memory found yet)";
        }
        return {{"content", {{{"type", "text"}, {"text", report}}}}};
    }

    if (action == "delete") {
        std::string key = args.value("key", "");
        if (key.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "❌ MEMORY_FAIL: key is required for delete."}}}}};
        }
        if (memory["facts"].contains(key)) {
            memory["facts"].erase(key);
        } else {
            auto& idx = memory["knowledge_index"];
            idx.erase(std::remove_if(idx.begin(), idx.end(), [&](const nlohmann::json& item) {
                return item.value("title", "") == key;
            }), idx.end());
        }
        std::ofstream fo(memoryPath);
        if (!fo.is_open()) return {{"error", "Could not open memory.json for writing"}};
        fo << memory.dump(4);
        return {{"content", {{{"type", "text"}, {"text", "✅ MEMORY_OK: deleted."}}}}};
    }

    return {{"content", {{{"type", "text"}, {"text", "❌ MEMORY_FAIL: unknown action."}}}}};
}

nlohmann::json InternalMCPClient::toolFileExplore(const nlohmann::json& args) {
    if (args.contains("query")) {
        return fileSearch(args);
    }
    return listDirTree(args);
}

std::string InternalMCPClient::executeCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
#ifdef _WIN32
    // Convert UTF-8 command to Wide String for Windows
    std::wstring wcmd;
    int len = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, NULL, 0);
    if (len > 0) {
        wcmd.resize(len - 1);
        MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], len);
    }
    FILE* pipePtr = _wpopen(wcmd.c_str(), L"r");
    auto pipeClose = _pclose;
#else
    FILE* pipePtr = popen(cmd.c_str(), "r");
    auto pipeClose = pclose;
#endif

    if (!pipePtr) {
        return "Error: popen() failed!";
    }
    
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipePtr) != nullptr) {
        result += buffer.data();
    }
    pipeClose(pipePtr);
    return sanitizeUtf8(result, 30000);
}

bool InternalMCPClient::commandExists(const std::string& cmd) {
#ifdef _WIN32
    std::string checkCmd = "where " + cmd + " >nul 2>&1";
#else
    std::string checkCmd = "command -v " + cmd + " >/dev/null 2>&1";
#endif
    return std::system(checkCmd.c_str()) == 0;
}

void InternalMCPClient::saveTasksToDisk() {
    fs::path tasksPath = globalDataPath / "tasks.json";
    nlohmann::json j = nlohmann::json::array();
    for (const auto& t : tasks) {
        j.push_back({
            {"id", t.id},
            {"description", t.description},
            {"pid", t.pid},
            {"isPeriodic", t.isPeriodic},
            {"interval", t.interval},
            {"startTime", t.startTime},
            {"logPath", t.logPath}
        });
    }
    std::ofstream f(tasksPath);
    if (f.is_open()) {
        f << j.dump(4);
    }
}

void InternalMCPClient::loadTasksFromDisk() {
    fs::path tasksPath = globalDataPath / "tasks.json";
    if (!fs::exists(tasksPath)) return;

    std::ifstream f(tasksPath);
    nlohmann::json j;
    try {
        f >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                BackgroundTask t;
                t.id = item["id"];
                t.description = item["description"];
                t.pid = item["pid"];
                t.isPeriodic = item["isPeriodic"];
                t.interval = item["interval"];
                t.startTime = item["startTime"];
                if (item.contains("logPath")) {
                    t.logPath = item["logPath"];
                }
                
                // Only re-add if the process is actually still alive
#ifdef _WIN32
                std::string checkCmd = "tasklist /FI \"PID eq " + std::to_string(t.pid) + "\" 2>nul";
                std::string output = executeCommand(checkCmd);
                if (output.find(std::to_string(t.pid)) != std::string::npos) {
                    tasks.push_back(t);
                }
#else
                if (kill(t.pid, 0) == 0) {
                    tasks.push_back(t);
                }
#endif
            }
        }
    } catch (...) {}
}
