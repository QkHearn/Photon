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
    toolHandlers["write"] = [this](const nlohmann::json& a) { return toolFileWrite(a); };
    toolHandlers["explore"] = [this](const nlohmann::json& a) { return toolFileExplore(a); };
    toolHandlers["context_read"] = [this](const nlohmann::json& a) { return contextRead(a); };
    toolHandlers["diagnostics_check"] = [this](const nlohmann::json& a) { return diagnosticsCheck(a); };
    toolHandlers["python_sandbox"] = [this](const nlohmann::json& a) { return pythonSandbox(a); };
    toolHandlers["pip_install"] = [this](const nlohmann::json& a) { return pipInstall(a); };
    toolHandlers["sequential_thinking"] = [this](const nlohmann::json& a) { return sequentialThinking(a); };
    toolHandlers["arch_visualize"] = [this](const nlohmann::json& a) { return archVisualize(a); };
    toolHandlers["bash_execute"] = [this](const nlohmann::json& a) { return bashExecute(a); };
    toolHandlers["code_ast_analyze"] = [this](const nlohmann::json& a) { return codeAstAnalyze(a); };
    toolHandlers["git_operations"] = [this](const nlohmann::json& a) { return gitOperations(a); };
    toolHandlers["web_fetch"] = [this](const nlohmann::json& a) { return webFetch(a); };
    toolHandlers["web_search"] = [this](const nlohmann::json& a) { return webSearch(a); };
    toolHandlers["harmony_search"] = [this](const nlohmann::json& a) { return harmonySearch(a); };
    toolHandlers["grep_search"] = [this](const nlohmann::json& a) { return grepSearch(a); };
    toolHandlers["diff_apply"] = [this](const nlohmann::json& a) { return fileWrite(a); };
    toolHandlers["file_undo"] = [this](const nlohmann::json& a) { return fileUndo(a); };
    toolHandlers["memory_store"] = [this](const nlohmann::json& a) { return memoryStore(a); };
    toolHandlers["memory_list"] = [this](const nlohmann::json& a) { return memoryList(a); };
    toolHandlers["memory_retrieve"] = [this](const nlohmann::json& a) { return memoryRetrieve(a); };
    toolHandlers["project_overview"] = [this](const nlohmann::json& a) { return projectOverview(a); };
    toolHandlers["symbol_search"] = [this](const nlohmann::json& a) { return symbolSearch(a); };
    toolHandlers["semantic_search"] = [this](const nlohmann::json& a) { return semanticSearch(a); };
    toolHandlers["lsp_definition"] = [this](const nlohmann::json& a) { return lspDefinition(a); };
    toolHandlers["lsp_references"] = [this](const nlohmann::json& a) { return lspReferences(a); };
    toolHandlers["generate_logic_map"] = [this](const nlohmann::json& a) { return generateLogicMap(a); };
    toolHandlers["resolve_relative_date"] = [this](const nlohmann::json& a) { return resolveRelativeDate(a); };
    toolHandlers["skill_read"] = [this](const nlohmann::json& a) { return skillRead(a); };
    toolHandlers["schedule"] = [this](const nlohmann::json& a) { return osScheduler(a); };
    toolHandlers["tasks"] = [this](const nlohmann::json& a) { return listTasks(a); };
    toolHandlers["cancel_task"] = [this](const nlohmann::json& a) { return cancelTask(a); };
    toolHandlers["read_task_log"] = [this](const nlohmann::json& a) { return readTaskLog(a); };
    toolHandlers["authorize"] = [this](const nlohmann::json& a) { return authorizeSession(a); };
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
        {"description", "Read file content. STRONGLY PREFER targeted reads using 'start_line' and 'end_line' or symbol-based 'query' to save tokens. "
                        "Full-file reads (only 'path') should be avoided for large files."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"start_line", {{"type", "integer"}, {"description", "Start line (1-based) for targeted read"}}},
                {"end_line", {{"type", "integer"}, {"description", "End line (1-based) for targeted read"}}},
                {"query", {{"type", "string"}, {"description", "Symbol or identifier to locate via the index for context read"}}},
                {"path", {{"type", "string"}, {"description", "Relative path of the file"}}},
                {"context_lines", {{"type", "integer"}, {"description", "Lines of context above and below the match (default: 20)"}}},
                {"max_matches", {{"type", "integer"}, {"description", "Maximum number of matches to return (default: 3)"}}},
                {"requests", {
                    {"type", "array"},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}}},
                            {"start_line", {{"type", "integer"}}},
                            {"end_line", {{"type", "integer"}}}
                        }},
                        {"required", {"path", "start_line", "end_line"}}
                    }}
                }}
            }}
        }}
    });

    // Consolidated Write Tool
    tools.push_back({
        {"name", "write"},
        {"description", "Modify file content. [POLICY] Surgical edits are MANDATORY. Full-file overwrites are FORBIDDEN for existing files. "
                        "Methods: 1) 'operation' (insert/replace/delete) with 'start_line'/'end_line' for precise line edits, or 2) 'search' and 'replace' for diff-style edits. "
                        "Full-file overwrites (only 'path' and 'content') are only allowed when creating NEW files."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}, {"description", "Line edit type for surgical modification"}}},
                {"start_line", {{"type", "integer"}, {"description", "Start line for surgical edit"}}},
                {"end_line", {{"type", "integer"}, {"description", "End line for surgical edit"}}},
                {"search", {{"type", "string"}, {"description", "Text to find for diff-style surgical edit"}}},
                {"replace", {{"type", "string"}, {"description", "Text to replace for diff-style surgical edit"}}},
                {"path", {{"type", "string"}, {"description", "Relative path to the file"}}},
                {"content", {{"type", "string"}, {"description", "Content for the edit or full file content"}}},
                {"edits", {
                    {"type", "array"},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"path", {{"type", "string"}}},
                            {"operation", {{"type", "string"}, {"enum", {"insert", "replace", "delete"}}}},
                            {"start_line", {{"type", "integer"}}},
                            {"end_line", {{"type", "integer"}}},
                            {"content", {{"type", "string"}}}
                        }},
                        {"required", {"path", "operation", "start_line"}}
                    }}
                }}
            }}
        }}
    });

    // Authorize Tool
    tools.push_back({
        {"name", "authorize"},
        {"description", "Authorize the current session to perform high-risk operations like file writing and bash execution. "
                        "This only needs to be called once per session."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {}}
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
            }}
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
            }}
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

    // Bash Execute Tool
    tools.push_back({
        {"name", "sequential_thinking"},
        {"description", "A tool for managing complex reasoning processes. It allows the agent to break down problems, maintain state across steps, and refine strategies as information is gathered. Use this when you need to perform multi-step planning or deep analysis."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"thought", {{"type", "string"}, {"description", "The current step's thinking content"}}},
                {"thoughtNumber", {{"type", "integer"}, {"description", "The current thinking step number"}}},
                {"totalThoughtNumber", {{"type", "integer"}, {"description", "Estimated total steps for this reasoning thread"}}},
                {"nextThoughtNeeded", {{"type", "boolean"}, {"description", "Whether another thinking step is required"}}},
                {"isRevision", {{"type", "boolean"}, {"description", "Set to true if this step revises a previous conclusion"}}}
            }},
            {"required", {"thought", "thoughtNumber", "nextThoughtNeeded"}}
        }}
    });

    tools.push_back({
        {"name", "arch_visualize"},
        {"description", "Generate system architecture diagrams using Graphviz. This tool accepts a DOT-format string and renders it into an image (PNG/SVG) in the workspace."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"dot_content", {{"type", "string"}, {"description", "The Graphviz DOT language content describing the diagram"}}},
                {"output_file", {{"type", "string"}, {"description", "The filename for the output image (e.g., arch.png)"}}},
                {"format", {{"type", "string"}, {"description", "The output format: png, svg, or pdf"}}}
            }},
            {"required", {"dot_content", "output_file"}}
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

    // Code AST Analyze Tool
    tools.push_back({
        {"name", "code_ast_analyze"},
        {"description", "Analyze the high-level structure (classes, methods, functions) of a file using Tree-sitter. Perfect for seeing the 'skeleton' of an entry point or module before diving into details."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}}
            }},
            {"required", {"path"}}
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

    // Grep Search Tool
    tools.push_back({
        {"name", "grep_search"},
        {"description", "Search for a pattern in all files within the workspace (like grep -rn)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"pattern", {{"type", "string"}, {"description", "The regex or string pattern to search for"}}}
            }},
            {"required", {"pattern"}}
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

    tools.push_back({
        {"name", "memory_store"},
        {"description", "Store information in Photon's long-term memory. Supports short facts (JSON) and long-form knowledge (Markdown)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"key", {{"type", "string"}, {"description", "The key/title for this memory"}}},
                {"value", {{"type", "string"}, {"description", "The content to remember"}}},
                {"type", {{"type", "string"}, {"enum", {"fact", "knowledge"}}, {"description", "Storage type: 'fact' for JSON (short), 'knowledge' for Markdown (long/structured)"}}}
            }},
            {"required", {"key", "value"}}
        }}
    });

    // Memory List Tool
    tools.push_back({
        {"name", "memory_list"},
        {"description", "List all stored facts and knowledge files in long-term memory."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", nlohmann::json::object()},
            {"required", nlohmann::json::array()}
        }}
    });

    // Memory Retrieve Tool
    tools.push_back({
        {"name", "memory_retrieve"},
        {"description", "Retrieve a value from Photon's long-term memory by key."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"key", {{"type", "string"}, {"description", "The key to search for"}}}
            }},
            {"required", {"key"}}
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

    // Symbol Search Tool
    tools.push_back({
        {"name", "symbol_search"},
        {"description", "Search for global symbols (classes, functions, methods) across the entire project. This uses a pre-built index and is extremely fast."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "The symbol name to search for"}}}
            }},
            {"required", {"query"}}
        }}
    });

    // Semantic Search Tool
    tools.push_back({
        {"name", "semantic_search"},
        {"description", "Perform a semantic search across the entire project, including code, documentation (Markdown), and stored knowledge. Use this when you don't know the exact symbol name but know the functionality or concept you're looking for."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "The concept, functionality, or question to search for"}}},
                {"top_k", {{"type", "integer"}, {"description", "Maximum number of results to return (default: 5)"}}}
            }},
            {"required", {"query"}}
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

    // Logic Map Tool
    tools.push_back({
        {"name", "generate_logic_map"},
        {"description", "Generate a dynamic call topology graph starting from an entry point (e.g., main). Uses LSP and Tree-sitter to trace the logic flow. Essential for understanding project architecture quickly."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"entry_symbol", {{"type", "string"}, {"description", "The name of the entry symbol (e.g., 'main', 'app', 'EntryComponent')"}}},
                {"max_depth", {{"type", "integer"}, {"description", "Maximum recursion depth for the call chain (default: 3)"}}}
            }},
            {"required", {"entry_symbol"}}
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

    auto startTime = std::chrono::high_resolution_clock::now();
    nlohmann::json result = it->second(arguments);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Add Telemetry to the result
    if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
        std::string telemetry = "\n\n[Telemetry] Execution time: " + std::to_string(duration) + "ms";
        if (result["content"][0].contains("text")) {
            std::string currentText = result["content"][0]["text"];
            result["content"][0]["text"] = currentText + telemetry;
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
        return {{"content", {{{"type", "text"}, {"text", "Error: File not found: " + relPathStr}}}}};
    }

    // Size check for token optimization
    std::uintmax_t fileSize = fs::file_size(fullPath);
    const std::uintmax_t MAX_READ_SIZE = 50000; // 50KB soft limit

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: Could not open file: " + relPathStr}}}}};
    }

    std::string contentStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // 记录读取时的哈希值，用于后续冲突检测
    fileReadHashes[relPathStr] = calculateHash(contentStr);

    if (fileSize > MAX_READ_SIZE) {
        // Optimization: Instead of just first 2000 chars, provide a summary of symbols + first/last parts
        std::string report = "⚠️ WARNING: File is too large (" + std::to_string(fileSize / 1024) + " KB).\n";
        report += "To save tokens, providing a structural summary. Use 'read_file_lines' or 'context_read' for details.\n\n";
        
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

    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(contentStr, 100000)}}}}};
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

    return {{"content", {{{"type", "text"}, {"text", "❌ Build issues found:\n\n" + filtered + "\n\n💡 Healing Strategy: Analyze the error patterns above. Use 'grep_search' or 'context_read' to locate the problematic code. If the error is in a file you recently modified, review your changes using 'git_operations' (diff) and apply a fix."}}}}};
}

nlohmann::json InternalMCPClient::contextRead(const nlohmann::json& args) {
    if (!symbolManager) {
        return {{"content", {{{"type", "text"}, {"text", "SymbolManager not initialized."}}}}};
    }

    std::string query = args.value("query", "");
    if (query.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: query is required."}}}}};
    }
    int contextLines = args.value("context_lines", 20);
    int maxMatches = args.value("max_matches", 3);
    if (contextLines < 0) contextLines = 0;
    if (maxMatches < 1) maxMatches = 1;
    std::string pathFilter = args.value("path", "");

    auto results = symbolManager->search(query);
    if (!pathFilter.empty()) {
        std::vector<SymbolManager::Symbol> filtered;
        for (const auto& s : results) {
            if (s.path == pathFilter) filtered.push_back(s);
        }
        results.swap(filtered);
    }
    if (results.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "No symbols found for: " + query}}}}};
    }

    std::unordered_set<std::string> hotFiles;
    if (isGitRepo) {
        std::string cmd = "git -C " + rootPath.u8string() + " status --porcelain";
        std::string output = executeCommand(cmd);
        std::stringstream ss(output);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.size() < 4) continue;
            std::string path = line.substr(3);
            auto arrow = path.find("->");
            if (arrow != std::string::npos) {
                path = path.substr(arrow + 2);
                while (!path.empty() && path[0] == ' ') path.erase(path.begin());
            }
            if (!path.empty()) hotFiles.insert(path);
        }
    }
    if (!lastFile.empty()) {
        hotFiles.insert(lastFile);
    }

    std::stable_partition(results.begin(), results.end(), [&](const SymbolManager::Symbol& s) {
        return hotFiles.find(s.path) != hotFiles.end();
    });

    auto readWindow = [&](const fs::path& filePath, int line, int ctx) -> std::string {
        std::ifstream file(filePath);
        if (!file.is_open()) return "";
        int start = std::max(1, line - ctx);
        int end = std::max(line + ctx, start);
        std::string out;
        std::string textLine;
        int current = 1;
        while (std::getline(file, textLine)) {
            if (current >= start && current <= end) {
                out += std::to_string(current) + "|" + textLine + "\n";
            }
            if (current > end) break;
            current++;
        }
        return out;
    };

    std::string report;
    int count = 0;
    for (const auto& s : results) {
        if (count >= maxMatches) break;
        fs::path fullPath = rootPath / fs::u8path(s.path);
        if (!fs::exists(fullPath)) continue;
        int startLine = std::max(1, s.line - contextLines);
        int endLine = s.line + contextLines;
        std::string window = readWindow(fullPath, s.line, contextLines);
        if (window.empty()) continue;
        std::string source = s.source.empty() ? "unknown" : s.source;
        report += "📍 Match [" + s.type + "|" + source + "] `" + s.name + "` in `" +
                  s.path + "`\n";
        report += "   └─ Exact Location: Line " + std::to_string(s.line) + 
                  " | Context Range: Lines " + std::to_string(startLine) + "-" + std::to_string(endLine) + "\n";
        report += "   └─ To read exact range: read_file_lines(path=\"" + s.path + 
                  "\", start_line=" + std::to_string(startLine) + ", end_line=" + std::to_string(endLine) + ")\n";
        report += "\n" + window + "\n";
        count++;
    }
    if (report.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "No readable matches found for: " + query}}}}};
    }
    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(report, 12000)}}}}};
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

nlohmann::json InternalMCPClient::sequentialThinking(const nlohmann::json& args) {
    int thoughtNumber = args.value("thoughtNumber", 1);
    int totalThoughtNumber = args.value("totalThoughtNumber", 0);
    std::string thought = args.value("thought", "");
    bool nextThoughtNeeded = args.value("nextThoughtNeeded", false);
    
    std::string status = "Thought " + std::to_string(thoughtNumber);
    if (totalThoughtNumber > 0) {
        status += " of " + std::to_string(totalThoughtNumber);
    }
    
    std::string result = "Successfully processed " + status + ". ";
    if (nextThoughtNeeded) {
        result += "Please proceed with your next thought.";
    } else {
        result += "Reasoning chain complete.";
    }

    return {{"content", {{{"type", "text"}, {"text", result}}}}};
}

nlohmann::json InternalMCPClient::archVisualize(const nlohmann::json& args) {
    std::string dotContent = args["dot_content"];
    std::string outputFile = args["output_file"];
    std::string format = args.value("format", "png");
    
    // Create a temporary DOT file
    std::string dotFileName = ".tmp_arch.dot";
    fs::path dotFilePath = rootPath / dotFileName;
    std::ofstream out(dotFilePath);
    if (!out) {
        return {{"content", {{{"type", "text"}, {"text", "Error: Failed to create temporary DOT file."}}}}};
    }
    out << dotContent;
    out.close();

    // Use Python to render the DOT file if graphviz command is not directly available
    // This script tries to use the 'graphviz' python library to render
    std::string pythonScript = 
        "import sys\n"
        "import os\n"
        "try:\n"
        "    import graphviz\n"
        "    # Remove extension from outputFile for graphviz render\n"
        "    base_name = '" + outputFile + "'.rsplit('.', 1)[0]\n"
        "    source = graphviz.Source.from_file('" + dotFileName + "')\n"
        "    output = source.render(filename=base_name, format='" + format + "', cleanup=True)\n"
        "    print(f'Diagram successfully rendered to {output}')\n"
        "except ImportError:\n"
        "    print('Error: Python \"graphviz\" library not found. Please install it using: pip_install graphviz')\n"
        "except Exception as e:\n"
        "    print(f'Error during rendering: {str(e)}')\n";

    nlohmann::json pythonArgs = {{"code", pythonScript}};
    nlohmann::json result = pythonSandbox(pythonArgs);

    // Clean up DOT file
    if (fs::exists(dotFilePath)) {
        fs::remove(dotFilePath);
    }

    return result;
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
            result += "\n*Tip: Use 'read_file_lines' with these line numbers for precise reading.*\n";
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
    
    result += "\n*Tip: Use 'read_file_lines' with these line numbers for precise reading.*";
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
                      "Instead, use `grep_search` or `semantic_search` to find similar patterns in the local codebase, "
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

nlohmann::json InternalMCPClient::grepSearch(const nlohmann::json& args) {
    std::string patternStr = args["pattern"];
    std::string results;

    if (isGitRepo) {
        // Use Git Grep for extremely fast content search (including untracked files)
        std::string cmd = "git -C " + rootPath.u8string() + " grep -n -i -I --untracked --context=1 \"" + patternStr + "\"";
        results = executeCommand(cmd);
        if (!results.empty()) {
            return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 8000)}}}}};
        }
    }

    // Fallback: Parallel search using std::async
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

    std::string lowerPattern = patternStr;
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);

    std::mutex resultsMutex;
    std::vector<std::future<void>> futures;

    // Process files in parallel
    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 2;
    size_t chunkSize = (targetFiles.size() + numThreads - 1) / numThreads;

    for (size_t i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i, chunkSize, lowerPattern]() {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, targetFiles.size());

            for (size_t j = start; j < end; ++j) {
                const auto& filePath = targetFiles[j];
                if (isBinary(filePath)) continue;

                std::ifstream file(filePath);
                if (!file.is_open()) continue;

                std::string line;
                int lineNum = 1;
                while (std::getline(file, line)) {
                    std::string lowerLine = line;
                    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

                    if (lowerLine.find(lowerPattern) != std::string::npos) {
                        std::lock_guard<std::mutex> lock(resultsMutex);
                        results += fs::relative(filePath, rootPath).generic_string() + ":" + std::to_string(lineNum) + ":" + line + "\n";
                    }
                    lineNum++;
                }
            }
        }));
    }

    for (auto& f : futures) f.wait();

    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 8000)}}}}};
}

nlohmann::json InternalMCPClient::readFileLines(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    int start = args["start_line"];
    int end = args["end_line"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) {
        return {{"content", {{{"type", "text"}, {"text", "Error: File not found: " + relPathStr}}}}};
    }

    if (start < 1) start = 1;
    if (end < start) end = start;

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: Could not open file: " + relPathStr}}}}};
    }

    std::string line;
    std::string content;
    content += "📄 File: `" + relPathStr + "` | Lines " + std::to_string(start) + "-" + std::to_string(end) + "\n";
    std::string separator;
    for(int i=0; i<60; ++i) separator += "─";
    content += separator + "\n";
    
    int current = 1;
    int totalLines = 0;
    int fileTotalLines = 0;
    while (std::getline(file, line)) {
        fileTotalLines++;
        if (current >= start && current <= end) {
            content += std::to_string(current) + " | " + line + "\n";
            totalLines++;
        }
        if (current > end) break;
        current++;
    }
    
    if (totalLines == 0) {
        content += "⚠️  No lines in range. ";
        if (fileTotalLines > 0) {
            content += "File has " + std::to_string(fileTotalLines) + " total line(s).\n";
            content += "💡 Tip: Use start_line=1, end_line=" + std::to_string(fileTotalLines) + " to read the entire file.\n";
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
    
    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(content, 8000)}}}}};
}

nlohmann::json InternalMCPClient::readBatchLines(const nlohmann::json& args) {
    if (!args.contains("requests") || !args["requests"].is_array()) {
        return {{"error", "Missing or invalid 'requests' array"}};
    }

    std::string report;
    int totalFilesProcessed = 0;

    for (const auto& req : args["requests"]) {
        std::string relPathStr = req.value("path", "");
        int start = req.value("start_line", 1);
        int end = req.value("end_line", 1);
        
        fs::path fullPath = rootPath / fs::u8path(relPathStr);
        if (!fs::exists(fullPath)) {
            report += "❌ File not found: " + relPathStr + "\n\n";
            continue;
        }

        std::ifstream file(fullPath);
        if (!file.is_open()) {
            report += "❌ Could not open file: " + relPathStr + "\n\n";
            continue;
        }

        if (start < 1) start = 1;
        if (end < start) end = start;

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
            report += "⚠️  No lines found in this range.\n";
        }
        report += separator + "\n\n";
        totalFilesProcessed++;
    }

    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(report, 12000)}}}}};
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

nlohmann::json InternalMCPClient::memoryStore(const nlohmann::json& args) {
    std::string key = args["key"];
    std::string value = args["value"];
    std::string type = args.value("type", "fact");
    
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

        // Also record in JSON index
        fs::path memoryPath = globalDataPath / "memory.json";
        nlohmann::json memory = nlohmann::json::object();
        if (fs::exists(memoryPath)) {
            std::ifstream fi(memoryPath);
            try { fi >> memory; } catch (...) {}
        }
        if (!memory.contains("knowledge_index")) memory["knowledge_index"] = nlohmann::json::array();
        memory["knowledge_index"].push_back({{"title", key}, {"timestamp", ss.str()}});
        
        std::ofstream fo(memoryPath);
        fo << memory.dump(4);
        
        return {{"content", {{{"type", "text"}, {"text", "Knowledge stored in knowledge.md and indexed."}}}}};
    } else {
        fs::path memoryPath = globalDataPath / "memory.json";
        nlohmann::json memory = nlohmann::json::object();
        if (fs::exists(memoryPath)) {
            std::ifstream f(memoryPath);
            if (f.is_open()) {
                try { f >> memory; } catch (...) {}
                f.close();
            }
        }

        if (!memory.contains("facts")) memory["facts"] = nlohmann::json::object();
        memory["facts"][key] = value;

        std::ofstream f(memoryPath);
        if (!f.is_open()) return {{"error", "Could not open memory.json for writing"}};
        f << memory.dump(4);
        f.close();

        return {{"content", {{{"type", "text"}, {"text", "Fact stored for key: " + key}}}}};
    }
}

nlohmann::json InternalMCPClient::memoryList(const nlohmann::json& args) {
    fs::path memoryPath = globalDataPath / "memory.json";
    std::string report = "Photon Long-term Memory Index:\n\n";
    
    if (fs::exists(memoryPath)) {
        std::ifstream f(memoryPath);
        nlohmann::json memory;
        try {
            f >> memory;
            if (memory.contains("facts") && !memory["facts"].empty()) {
                report += "### [Facts (JSON)]\n";
                for (auto& [key, val] : memory["facts"].items()) {
                    report += "- " + key + "\n";
                }
                report += "\n";
            }
            if (memory.contains("knowledge_index") && !memory["knowledge_index"].empty()) {
                report += "### [Knowledge (Markdown)]\n";
                for (const auto& item : memory["knowledge_index"]) {
                    report += "- " + item.value("title", "Untitled") + " (" + item.value("timestamp", "") + ")\n";
                }
            }
        } catch (...) {
            return {{"error", "Memory file corrupted"}};
        }
    } else {
        report += "(No memory found yet)";
    }
    
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::memoryRetrieve(const nlohmann::json& args) {
    std::string key = args["key"];
    fs::path memoryPath = globalDataPath / "memory.json";

    if (!fs::exists(memoryPath)) return {{"error", "No memory found yet."}};

    std::ifstream f(memoryPath);
    nlohmann::json memory;
    try { f >> memory; } catch (...) { return {{"error", "Memory file corrupted"}}; }
    f.close();

    if (memory.contains("facts") && memory["facts"].contains(key)) {
        return {{"content", {{{"type", "text"}, {"text", memory["facts"][key].get<std::string>()}}}}};
    } else {
        // If not in facts, check if it's a request for the whole knowledge file
        if (key == "knowledge.md" || key == "knowledge") {
            fs::path kbPath = globalDataPath / "knowledge.md";
            if (fs::exists(kbPath)) {
                std::ifstream kbf(kbPath);
                std::string content((std::istreambuf_iterator<char>(kbf)), std::istreambuf_iterator<char>());
                return {{"content", {{{"type", "text"}, {"text", content}}}}};
            }
        }
        return {{"content", {{{"type", "text"}, {"text", "Key not found in facts. Use memory_list to see available knowledge titles."}}}}};
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

    report += "\n*Strategy: Start by using 'code_ast_analyze' on these core files instead of reading them all.*";
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::symbolSearch(const nlohmann::json& args) {
    if (!symbolManager) return {{"error", "SymbolManager not initialized"}};
    
    std::string query = args["query"];
    auto results = symbolManager->search(query);
    
    if (results.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "No symbols found matching: " + query}}}}};
    }
    
    std::string report = "Found " + std::to_string(results.size()) + " matches:\n";
    for (const auto& s : results) {
        std::string source = s.source.empty() ? "unknown" : s.source;
        report += "📍 [" + s.type + "|" + source + "] `" + s.name + "`\n";
        report += "   └─ File: `" + s.path + "` | Line: " + std::to_string(s.line) + "\n";
        report += "   └─ Quick read: context_read(query=\"" + s.name + "\", path=\"" + s.path + "\")\n";
        report += "   └─ Exact read: read_file_lines(path=\"" + s.path + 
                  "\", start_line=" + std::to_string(std::max(1, s.line - 10)) + 
                  ", end_line=" + std::to_string(s.line + 10) + ")\n\n";
    }
    return {{"content", {{{"type", "text"}, {"text", report}}}}};
}

nlohmann::json InternalMCPClient::semanticSearch(const nlohmann::json& args) {
    if (!semanticManager) {
        return {{"content", {{{"type", "text"}, {"text", "SemanticManager not initialized."}}}}};
    }

    std::string query = args.value("query", "");
    if (query.empty()) {
        return {{"content", {{{"type", "text"}, {"text", "Error: query is required."}}}}};
    }
    int topK = args.value("top_k", 5);

    auto results = semanticManager->search(query, topK);
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
        
        // Preview content
        std::string preview = chunk.content;
        if (preview.length() > 300) preview = preview.substr(0, 300) + "...";
        report += "   > " + preview + "\n\n";
    }

    report += "*Tip: Use 'read_file_lines' or 'context_read' to examine the full context of these matches.*";
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
        report += "   └─ Read context: read_file_lines(path=\"" + relPath + 
                  "\", start_line=" + std::to_string(contextStart) + 
                  ", end_line=" + std::to_string(contextEnd) + ")\n\n";
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
        report += "   └─ Quick view: read_file_lines(path=\"" + displayPath + 
                  "\", start_line=" + std::to_string(contextStart) + 
                  ", end_line=" + std::to_string(contextEnd) + ")\n\n";
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
    memoryStore({{"key", "task_log_" + taskId}, {"value", "Scheduled: " + type + " - " + payload + " (Logs: " + logPath.u8string() + ")"}});

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
    if (args.contains("query")) {
        return contextRead(args);
    }
    if (args.contains("requests")) {
        return readBatchLines(args);
    }
    if (args.contains("start_line") || args.contains("end_line")) {
        return readFileLines(args);
    }
    return fileRead(args);
}

nlohmann::json InternalMCPClient::toolFileWrite(const nlohmann::json& args) {
    bool highRisk = false;
    std::string targetPath;
    if (args.contains("path")) {
        targetPath = args["path"];
        highRisk = isHighRiskPath(targetPath);
    } else if (args.contains("edits")) {
        for (const auto& edit : args["edits"]) {
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

    // Level 2 Logging: Log normal file writes
    if (!targetPath.empty()) {
        Logger::getInstance().action("Writing to file: " + targetPath);
    }

    if (args.contains("edits")) {
        return editBatchLines(args);
    }
    if (args.contains("operation")) {
        return fileEditLines(args);
    }
    if (args.contains("search") && args.contains("replace")) {
        return diffApply(args);
    }
    return fileWrite(args);
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
