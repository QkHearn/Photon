#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "InternalMCPClient.h"
#include <algorithm>
#include <regex>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include <future>
#include <thread>
#include <mutex>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

InternalMCPClient::InternalMCPClient(const std::string& rootPathStr) {
    this->rootPath = fs::u8path(rootPathStr);
    isGitRepo = checkGitRepo();
    registerTools();
}

void InternalMCPClient::registerTools() {
    toolHandlers["file_search"] = [this](const nlohmann::json& a) { return fileSearch(a); };
    toolHandlers["file_read"] = [this](const nlohmann::json& a) { return fileRead(a); };
    toolHandlers["file_write"] = [this](const nlohmann::json& a) { return fileWrite(a); };
    toolHandlers["python_sandbox"] = [this](const nlohmann::json& a) { return pythonSandbox(a); };
    toolHandlers["bash_execute"] = [this](const nlohmann::json& a) { return bashExecute(a); };
    toolHandlers["code_ast_analyze"] = [this](const nlohmann::json& a) { return codeAstAnalyze(a); };
    toolHandlers["git_operations"] = [this](const nlohmann::json& a) { return gitOperations(a); };
    toolHandlers["web_fetch"] = [this](const nlohmann::json& a) { return webFetch(a); };
    toolHandlers["web_search"] = [this](const nlohmann::json& a) { return webSearch(a); };
    toolHandlers["harmony_search"] = [this](const nlohmann::json& a) { return harmonySearch(a); };
    toolHandlers["grep_search"] = [this](const nlohmann::json& a) { return grepSearch(a); };
    toolHandlers["read_file_lines"] = [this](const nlohmann::json& a) { return readFileLines(a); };
    toolHandlers["list_dir_tree"] = [this](const nlohmann::json& a) { return listDirTree(a); };
    toolHandlers["diff_apply"] = [this](const nlohmann::json& a) { return diffApply(a); };
    toolHandlers["file_undo"] = [this](const nlohmann::json& a) { return fileUndo(a); };
    toolHandlers["memory_store"] = [this](const nlohmann::json& a) { return memoryStore(a); };
    toolHandlers["memory_retrieve"] = [this](const nlohmann::json& a) { return memoryRetrieve(a); };
    toolHandlers["resolve_relative_date"] = [this](const nlohmann::json& a) { return resolveRelativeDate(a); };
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

    // File Search Tool
    tools.push_back({
        {"name", "file_search"},
        {"description", "Search for files in the workspace by name or content."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "The search query (filename or content snippet)"}}}
            }},
            {"required", {"query"}}
        }}
    });

    // File Read Tool
    tools.push_back({
        {"name", "file_read"},
        {"description", "Read the content of a specific file."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}}
            }},
            {"required", {"path"}}
        }}
    });

    // File Write Tool
    tools.push_back({
        {"name", "file_write"},
        {"description", "Write or overwrite a file with specific content."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}},
                {"content", {{"type", "string"}, {"description", "The content to write"}}}
            }},
            {"required", {"path", "content"}}
        }}
    });

    // Python Sandbox Tool
    tools.push_back({
        {"name", "python_sandbox"},
        {"description", "Execute Python code and get the output. This tool has write access to the current directory."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"code", {{"type", "string"}, {"description", "The Python code to execute"}}}
            }},
            {"required", {"code"}}
        }}
    });

    // Bash Execute Tool
    tools.push_back({
        {"name", "bash_execute"},
        {"description", "Execute a bash command in the workspace."},
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
        {"description", "Extract classes and function signatures from a code file (C++/Python) to understand its structure without reading full content."},
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
        {"description", "Search HarmonyOS developer documentation and community for technical information."},
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

    // Read File Lines Tool
    tools.push_back({
        {"name", "read_file_lines"},
        {"description", "Read specific lines from a file."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}},
                {"start_line", {{"type", "integer"}, {"description", "The starting line number (1-based)"}}},
                {"end_line", {{"type", "integer"}, {"description", "The ending line number (inclusive)"}}}
            }},
            {"required", {"path", "start_line", "end_line"}}
        }}
    });

    // List Dir Tree Tool
    tools.push_back({
        {"name", "list_dir_tree"},
        {"description", "Show the directory structure of the workspace as a tree."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to start (default: root)"}}},
                {"depth", {{"type", "integer"}, {"description", "Maximum depth to show (default: 3)"}}}
            }}
        }}
    });

    // Diff Apply Tool
    tools.push_back({
        {"name", "diff_apply"},
        {"description", "Apply a search-and-replace style change to a file. Automatically creates a backup for undo."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "The relative path to the file"}}},
                {"search", {{"type", "string"}, {"description", "The exact text to find in the file"}}},
                {"replace", {{"type", "string"}, {"description", "The text to replace it with"}}}
            }},
            {"required", {"path", "search", "replace"}}
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

    // Memory Store Tool
    tools.push_back({
        {"name", "memory_store"},
        {"description", "Store a key-value pair in Photon's long-term memory (.photon/memory.json)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"key", {{"type", "string"}, {"description", "The key to identify this memory"}}},
                {"value", {{"type", "string"}, {"description", "The content to remember"}}}
            }},
            {"required", {"key", "value"}}
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

nlohmann::json InternalMCPClient::fileRead(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) {
        return {{"error", "File not found: " + relPathStr}};
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"error", "Could not open file: " + relPathStr}};
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(content, 100000)}}}}};
}

nlohmann::json InternalMCPClient::fileWrite(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    std::string content = args["content"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    try {
        // Backup before writing
        backupFile(relPathStr);
        
        // Ensure directory exists
        fs::create_directories(fullPath.parent_path());
        
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            return {{"error", "Could not open file for writing: " + relPathStr}};
        }
        file << content;
        file.close();
        return {{"content", {{{"type", "text"}, {"text", "Successfully wrote to " + relPathStr}}}}};
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
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
        "/etc/", "/dev/", "/proc/", "/sys/", "/var/", "/root/", "/home/",
        "c:\\windows", "c:\\users", "c:\\program files"
    };

    for (const auto& dir : systemDirs) {
        if (lowerCmd.find(dir) != std::string::npos) return false;
    }

    return true;
}

nlohmann::json InternalMCPClient::pythonSandbox(const nlohmann::json& args) {
    std::string code = args["code"];
    
    if (!isCommandSafe(code)) {
        return {{"error", "Security Alert: Python code contains potentially dangerous patterns or system path access."}};
    }
    
    // Create a temporary python file
    std::string tmpFile = "tmp_sandbox.py";
    std::ofstream out(tmpFile);
    out << code;
    out.close();

    std::string pythonCmd = "python3";
#ifdef _WIN32
    // Check if python3 exists, if not fallback to python on Windows
    if (executeCommand("python3 --version").find("not found") != std::string::npos || 
        executeCommand("python3 --version").empty()) {
        pythonCmd = "python";
    }
#endif

    std::string output = executeCommand(pythonCmd + " " + tmpFile + " 2>&1");
    
    // Clean up
    fs::remove(tmpFile);

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::bashExecute(const nlohmann::json& args) {
    std::string command = args["command"];

    if (!isCommandSafe(command)) {
        return {{"error", "Security Alert: Command blocked by Photon Guard. Restricted keywords or paths detected."}};
    }

#ifdef _WIN32
    // On Windows, bash might not be available. Try to use cmd /c if it looks like a simple command,
    // but the tool is called 'bash_execute', so we'll try to use 'sh -c' or 'bash -c' first.
    std::string shellCmd = "sh -c \"" + command + "\"";
    // Check if sh exists
    if (executeCommand("sh --version").find("not found") != std::string::npos || 
        executeCommand("sh --version").empty()) {
        shellCmd = "cmd /c " + command;
    }
    std::string output = executeCommand(shellCmd + " 2>&1");
#else
    std::string output = executeCommand(command + " 2>&1");
#endif
    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::codeAstAnalyze(const nlohmann::json& args) {
    std::string relPathStr = args["path"];
    fs::path fullPath = rootPath / fs::u8path(relPathStr);

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    std::ifstream file(fullPath);
    std::string line;
    std::string result = "AST Analysis for " + relPathStr + ":\n";

    // More robust regex for C++ and Python
    // C++: handle namespaces, templates, and complex signatures
    static const std::regex cppClass(R"raw((class|struct)\s+([A-Za-z0-9_]+)(\s*:\s*[^{]+)?\s*\{)raw");
    static const std::regex cppFunc(R"raw(([A-Za-z0-9_<>, :*&]+)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*(const|override|final|noexcept)*\s*(\{|;))raw");
    // Python: handle classes and methods
    static const std::regex pyDef(R"raw(^\s*(def|async def)\s+([A-Za-z0-9_]+)\s*\()raw");
    static const std::regex pyClass(R"raw(^\s*class\s+([A-Za-z0-9_]+)\s*[:\(])raw");

    while (std::getline(file, line)) {
        // Simple comment skipping
        if (line.find("//") != std::string::npos && line.find("//") < 5) continue;
        if (line.find("#") != std::string::npos && line.find("#") < 5) continue;

        std::smatch match;
        if (std::regex_search(line, match, cppClass) || std::regex_search(line, match, pyClass)) {
            result += "[Class/Struct] " + match[2].str() + "\n";
        } else if (std::regex_search(line, match, cppFunc)) {
            result += "  [C++ Function] " + match[2].str() + " (returns " + match[1].str() + ")\n";
        } else if (std::regex_search(line, match, pyDef)) {
            result += "  [Py Function] " + match[2].str() + "\n";
        }
    }
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

    return {{"error", "Search failed: All engines blocked the request or triggered CAPTCHA. Please try again later or use a different query."}};
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
                if (firstResult.contains("developerInfos") && firstResult["developerInfos"].is_array()) {
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
                    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 20000)}}}}};
                }
            }
            return {{"content", {{{"type", "text"}, {"text", "No results found for: " + query}}}}};
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

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    std::ifstream file(fullPath);
    std::string line;
    std::string content;
    int current = 1;
    while (std::getline(file, line)) {
        if (current >= start && current <= end) {
            content += std::to_string(current) + "|" + line + "\n";
        }
        if (current > end) break;
        current++;
    }
    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(content, 8000)}}}}};
}

nlohmann::json InternalMCPClient::listDirTree(const nlohmann::json& args) {
    std::string subPathStr = args.value("path", "");
    int maxDepth = args.value("depth", 3);
    fs::path startPath = rootPath / fs::u8path(subPathStr);

    if (!fs::exists(startPath)) return {{"error", "Path not found"}};

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
    fs::path memoryPath = rootPath / ".photon" / "memory.json";

    nlohmann::json memory = nlohmann::json::object();
    if (fs::exists(memoryPath)) {
        std::ifstream f(memoryPath);
        if (f.is_open()) {
            try { f >> memory; } catch (...) {}
            f.close();
        }
    }

    memory[key] = value;

    std::ofstream f(memoryPath);
    if (!f.is_open()) return {{"error", "Could not open memory.json for writing"}};
    f << memory.dump(4);
    f.close();

    return {{"content", {{{"type", "text"}, {"text", "Memory stored for key: " + key}}}}};
}

nlohmann::json InternalMCPClient::memoryRetrieve(const nlohmann::json& args) {
    std::string key = args["key"];
    fs::path memoryPath = rootPath / ".photon" / "memory.json";

    if (!fs::exists(memoryPath)) return {{"error", "No memory found yet."}};

    std::ifstream f(memoryPath);
    nlohmann::json memory;
    try { f >> memory; } catch (...) { return {{"error", "Memory file corrupted"}}; }
    f.close();

    if (memory.contains(key)) {
        return {{"content", {{{"type", "text"}, {"text", memory[key].get<std::string>()}}}}};
    } else {
        return {{"content", {{{"type", "text"}, {"text", "Key not found in memory."}}}}};
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
    fs::create_directories(rootPath / ".photon" / "backups");
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
