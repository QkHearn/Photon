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

InternalMCPClient::InternalMCPClient(const std::string& rootPath) : rootPath(rootPath) {}

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
    std::string p = path.string();
    return p.find("/.git/") != std::string::npos || 
           p.find("/node_modules/") != std::string::npos || 
           p.find("/build/") != std::string::npos ||
           p.find("/.idea/") != std::string::npos ||
           p.find("/cmake-build-") != std::string::npos;
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
                std::string tagLower = currentTag;
                std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
                
                if (tagLower == "script" || tagLower == "style") {
                    inScriptOrStyle = true;
                } else if (tagLower == "/script" || tagLower == "/style") {
                    inScriptOrStyle = false;
                } else if (tagLower == "p" || tagLower == "div" || tagLower == "br" || tagLower == "li" || tagLower == "tr") {
                    result += '\n';
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

    // Fast entity replacement without regex
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
    auto startTime = std::chrono::high_resolution_clock::now();
    nlohmann::json result;

    if (name == "file_search") {
        result = fileSearch(arguments);
    } else if (name == "file_read") {
        result = fileRead(arguments);
    } else if (name == "file_write") {
        result = fileWrite(arguments);
    } else if (name == "python_sandbox") {
        result = pythonSandbox(arguments);
    } else if (name == "bash_execute") {
        result = bashExecute(arguments);
    } else if (name == "code_ast_analyze") {
        result = codeAstAnalyze(arguments);
    } else if (name == "git_operations") {
        result = gitOperations(arguments);
    } else if (name == "web_fetch") {
        result = webFetch(arguments);
    } else if (name == "web_search") {
        result = webSearch(arguments);
    } else if (name == "harmony_search") {
        result = harmonySearch(arguments);
    } else if (name == "grep_search") {
        result = grepSearch(arguments);
    } else if (name == "read_file_lines") {
        result = readFileLines(arguments);
    } else if (name == "list_dir_tree") {
        result = listDirTree(arguments);
    } else if (name == "diff_apply") {
        result = diffApply(arguments);
    } else if (name == "file_undo") {
        result = fileUndo(arguments);
    } else if (name == "memory_store") {
        result = memoryStore(arguments);
    } else if (name == "memory_retrieve") {
        result = memoryRetrieve(arguments);
    } else {
        return {{"error", "Tool not found"}};
    }

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

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (shouldIgnore(entry.path())) continue;
            if (entry.is_regular_file()) {
                std::string relPath = fs::relative(entry.path(), rootPath).string();
                
                // Search in filename
                if (relPath.find(query) != std::string::npos) {
                    matches.push_back(relPath);
                    continue;
                }

                // Search in content (simple text search)
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::string line;
                    while (std::getline(file, line)) {
                        if (line.find(query) != std::string::npos) {
                            matches.push_back(relPath);
                            break;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }

    return {{"content", {{{"type", "text"}, {"text", nlohmann::json(matches).dump(2)}}}}};
}

nlohmann::json InternalMCPClient::fileRead(const nlohmann::json& args) {
    std::string relPath = args["path"];
    fs::path fullPath = fs::path(rootPath) / relPath;

    if (!fs::exists(fullPath)) {
        return {{"error", "File not found: " + relPath}};
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return {{"error", "Could not open file: " + relPath}};
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return {{"content", {{{"type", "text"}, {"text", content}}}}};
}

nlohmann::json InternalMCPClient::fileWrite(const nlohmann::json& args) {
    std::string relPath = args["path"];
    std::string content = args["content"];
    fs::path fullPath = fs::path(rootPath) / relPath;

    try {
        // Backup before writing
        backupFile(relPath);
        
        // Ensure directory exists
        fs::create_directories(fullPath.parent_path());
        
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            return {{"error", "Could not open file for writing: " + relPath}};
        }
        file << content;
        file.close();
        return {{"content", {{{"type", "text"}, {"text", "Successfully wrote to " + relPath}}}}};
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

nlohmann::json InternalMCPClient::pythonSandbox(const nlohmann::json& args) {
    std::string code = args["code"];
    
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
    std::string relPath = args["path"];
    fs::path fullPath = fs::path(rootPath) / relPath;

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    std::ifstream file(fullPath);
    std::string line;
    std::string result = "AST Analysis for " + relPath + ":\n";

    // More robust regex for C++ and Python
    // C++: handle namespaces, templates, and complex signatures
    std::regex cppClass(R"((class|struct)\s+([A-Za-z0-9_]+)(\s*:\s*[^{]+)?\s*\{)");
    std::regex cppFunc(R"(([A-Za-z0-9_<>, :*&]+)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*(const|override|final|noexcept)*\s*(\{|;))");
    // Python: handle classes and methods
    std::regex pyDef(R"(^\s*(def|async def)\s+([A-Za-z0-9_]+)\s*\()");
    std::regex pyClass(R"(^\s*class\s+([A-Za-z0-9_]+)\s*[:\(])");

    while (std::getline(file, line)) {
        // Simple comment skipping
        if (line.find("//") != std::string::npos && line.find("//") < 5) continue;
        if (line.find("#") != std::string::npos && line.find("#") < 5) continue;

        // Static regex for AST to avoid re-compiling in large file loops
        static const std::regex cppClass(R"((class|struct)\s+([A-Za-z0-9_]+)(\s*:\s*[^{]+)?\s*\{)");
        static const std::regex cppFunc(R"(([A-Za-z0-9_<>, :*&]+)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*(const|override|final|noexcept)*\s*(\{|;))");
        static const std::regex pyDef(R"(^\s*(def|async def)\s+([A-Za-z0-9_]+)\s*\()");
        static const std::regex pyClass(R"(^\s*class\s+([A-Za-z0-9_]+)\s*[:\(])");

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
    std::string cmd = "git -C " + rootPath + " ";
    
    if (op == "status") cmd += "status";
    else if (op == "diff") cmd += "diff";
    else if (op == "log") cmd += "log --oneline -n 10";
    else if (op == "commit") {
        if (!args.contains("message")) return {{"error", "Commit message required"}};
        cmd += "add . && git -C " + rootPath + " commit -m \"" + args["message"].get<std::string>() + "\"";
    }

    std::string output = executeCommand(cmd + " 2>&1");
    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::webFetch(const nlohmann::json& args) {
    std::string url = args["url"];
    
    // Simple URL parser (only handles https://host/path)
    std::regex urlRegex(R"(https?://([^/]+)(/.*)?)");
    std::smatch match;
    if (!std::regex_match(url, match, urlRegex)) {
        return {{"error", "Invalid URL format. Use https://host/path"}};
    }

    std::string host = match[1];
    std::string path = match[2].length() > 0 ? match[2].str() : "/";

    try {
        httplib::Client cli("https://" + host);
        cli.set_follow_location(true);
        auto res = cli.Get(path.c_str());

        if (res && res->status == 200) {
            // Clean HTML and return sanitized body
            std::string cleaned = cleanHtml(res->body);
            // Increased limit to 15000 chars for richer context
            std::string body = sanitizeUtf8(cleaned, 15000);
            return {{"content", {{{"type", "text"}, {"text", body}}}}};
        } else {
            return {{"error", "Failed to fetch: " + (res ? std::to_string(res->status) : "Unknown error")}};
        }
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

nlohmann::json InternalMCPClient::webSearch(const nlohmann::json& args) {
    std::string query = args["query"];
    
    // DuckDuckGo HTML version
    std::string host = "html.duckduckgo.com";
    std::string path = "/html/?q=" + urlEncode(query); 

    try {
        httplib::Client cli(host);
        cli.set_follow_location(true);
        
        auto res = cli.Get(path.c_str());
        if (!res || res->status != 200) {
            return {{"error", "Failed to reach DuckDuckGo"}};
        }

        std::string html = res->body;
        std::string results;
        
        // Static regex compiled only once for extreme performance
        static const std::regex entry_regex("result__a\"\\s+href=\"([^\"]+)\">([\\s\\S]*?)</a>[\\s\\S]*?result__snippet\"[^>]*>([\\s\\S]*?)</a>");
        static const std::regex fallback_regex("result__a\"\\s+href=\"([^\"]+)\">([^<]+)</a>");
        
        auto words_begin = std::sregex_iterator(html.begin(), html.end(), entry_regex);
        auto words_end = std::sregex_iterator();

        int count = 0;
        for (std::sregex_iterator i = words_begin; i != words_end && count < 5; ++i) {
            std::smatch match = *i;
            std::string link = match[1].str();
            std::string title = cleanHtml(match[2].str());
            std::string snippet = cleanHtml(match[3].str());

            // Clean DuckDuckGo redirect links
            size_t uddg_pos = link.find("uddg=");
            if (uddg_pos != std::string::npos) {
                link = link.substr(uddg_pos + 5);
                size_t amp_pos = link.find("&");
                if (amp_pos != std::string::npos) {
                    link = link.substr(0, amp_pos);
                }
                // Decode the extracted link
                try {
                    link = urlDecode(link);
                } catch (...) {}
            }

            results += "Title: " + sanitizeUtf8(title, 300) + "\n";
            results += "Link: " + link + "\n";
            results += "Snippet: " + sanitizeUtf8(snippet, 1000) + "\n\n";
            count++;
        }

        if (results.empty()) {
            // Fallback to simpler regex if snippet regex fails
            auto simple_begin = std::sregex_iterator(html.begin(), html.end(), fallback_regex);
            for (std::sregex_iterator i = simple_begin; i != words_end && count < 5; ++i) {
                std::smatch match = *i;
                results += "Title: " + sanitizeUtf8(match[2].str(), 200) + "\n";
                results += "Link: " + match[1].str() + "\n\n";
                count++;
            }
        }

        if (results.empty()) {
            return {{"content", {{{"type", "text"}, {"text", "No results found or parsing failed."}}}}};
        }

        return {{"content", {{{"type", "text"}, {"text", results}}}}};
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

nlohmann::json InternalMCPClient::harmonySearch(const nlohmann::json& args) {
    std::string query = args["query"];
    
    // Generate timestamp YYYYMMDDHHMMSS
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
        {"deviceId", "ESN"},
        {"deviceType", "1"},
        {"language", "zh"},
        {"country", "CN"},
        {"keyWord", query},
        {"requestOrgin", 5},
        {"ts", ts},
        {"developerVertical", {
            {"category", 1},
            {"language", "zh"},
            {"catalog", "harmonyos-references"},
            {"searchSubTitle", 0},
            {"scene", 2},
            {"subType", 4}
        }},
        {"cutPage", {
            {"offset", 0},
            {"length", 5}
        }}
    };

    try {
        httplib::Client cli("https://svc-drcn.developer.huawei.com");
        cli.set_follow_location(true);
        
        auto res = cli.Post("/community/servlet/consumer/partnerCommunityService/developer/search", 
                           body.dump(), "application/json");

        if (res && res->status == 200) {
            auto jsonRes = nlohmann::json::parse(res->body);
            std::string summaryStr = "HarmonyOS Search Results for '" + query + "':\n\n";
            
            if (jsonRes.contains("data") && jsonRes["data"].contains("list") && jsonRes["data"]["list"].is_array()) {
                for (const auto& item : jsonRes["data"]["list"]) {
                    std::string title = item.value("name", ""); 
                    if (title.empty()) title = item.value("title", "No Title");
                    
                    title = sanitizeUtf8(title, 200);
                    
                    std::string link = item.value("url", "");
                    if (!link.empty() && link[0] == '/') {
                        link = "https://developer.huawei.com" + link;
                    }

                    std::string digest = item.value("digest", "");
                    if (digest.empty()) digest = item.value("content", "");
                    digest = cleanHtml(digest); // Use improved cleaner
                    digest = sanitizeUtf8(digest, 1000);
                    
                    summaryStr += "Title: " + title + "\n";
                    summaryStr += "Link: " + link + "\n";
                    if (!digest.empty()) summaryStr += "Snippet: " + digest + "\n";
                    summaryStr += "\n";
                }
            } else {
                summaryStr += "No results found or unexpected response format.";
            }
            return {{"content", {{{"type", "text"}, {"text", summaryStr}}}}};
        } else {
            return {{"error", "Failed to fetch HarmonyOS results: " + (res ? std::to_string(res->status) : "Unknown error")}};
        }
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

nlohmann::json InternalMCPClient::grepSearch(const nlohmann::json& args) {
    std::string patternStr = args["pattern"];
    std::string results;
    try {
        // Use static cache or at least compile pattern once outside the loop
        std::regex pattern(patternStr, std::regex::icase);
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (shouldIgnore(entry.path())) continue;
            if (entry.is_regular_file()) {
                std::ifstream file(entry.path());
                std::string line;
                int lineNum = 1;
                while (std::getline(file, line)) {
                    if (std::regex_search(line, pattern)) {
                        results += fs::relative(entry.path(), rootPath).string() + ":" + std::to_string(lineNum) + ":" + line + "\n";
                    }
                    lineNum++;
                }
            }
        }
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
    return {{"content", {{{"type", "text"}, {"text", sanitizeUtf8(results, 5000)}}}}};
}

nlohmann::json InternalMCPClient::readFileLines(const nlohmann::json& args) {
    std::string relPath = args["path"];
    int start = args["start_line"];
    int end = args["end_line"];
    fs::path fullPath = fs::path(rootPath) / relPath;

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
    std::string subPath = args.value("path", "");
    int maxDepth = args.value("depth", 3);
    fs::path startPath = fs::path(rootPath) / subPath;

    if (!fs::exists(startPath)) return {{"error", "Path not found"}};

    std::string tree;
    for (const auto& entry : fs::recursive_directory_iterator(startPath)) {
        auto rel = fs::relative(entry.path(), startPath);
        int depth = static_cast<int>(std::distance(rel.begin(), rel.end()));
        if (depth > maxDepth) continue;

        // Skip common ignore dirs
        std::string name = entry.path().filename().string();
        if (name == ".git" || name == "build" || name == ".idea" || name == "node_modules") continue;

        for (int i = 0; i < depth - 1; ++i) tree += "  ";
        tree += (depth > 0 ? "└── " : "") + name + (entry.is_directory() ? "/" : "") + "\n";
    }
    return {{"content", {{{"type", "text"}, {"text", tree}}}}};
}

nlohmann::json InternalMCPClient::diffApply(const nlohmann::json& args) {
    std::string relPath = args["path"];
    std::string search = args["search"];
    std::string replace = args["replace"];
    fs::path fullPath = fs::path(rootPath) / relPath;

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    // Backup before modifying
    backupFile(relPath);

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

    return {{"content", {{{"type", "text"}, {"text", "Successfully applied change to " + relPath}}}}};
}

nlohmann::json InternalMCPClient::fileUndo(const nlohmann::json& args) {
    std::string relPath = args["path"];
    fs::path fullPath = fs::path(rootPath) / relPath;
    fs::path backupPath = fs::path(rootPath) / ".photon" / "backups" / relPath;

    if (!fs::exists(backupPath)) {
        return {{"error", "No backup found for file in .photon/backups: " + relPath}};
    }

    try {
        fs::copy_file(backupPath, fullPath, fs::copy_options::overwrite_existing);
        return {{"content", {{{"type", "text"}, {"text", "Successfully restored " + relPath + " from .photon/backups."}}}}};
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

nlohmann::json InternalMCPClient::memoryStore(const nlohmann::json& args) {
    std::string key = args["key"];
    std::string value = args["value"];
    fs::path memoryPath = fs::path(rootPath) / ".photon" / "memory.json";

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
    fs::path memoryPath = fs::path(rootPath) / ".photon" / "memory.json";

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

void InternalMCPClient::backupFile(const std::string& relPath) {
    lastFile = relPath; // Track last modified file
    fs::path fullPath = fs::path(rootPath) / relPath;
    if (fs::exists(fullPath)) {
        fs::path backupPath = fs::path(rootPath) / ".photon" / "backups" / relPath;
        fs::create_directories(backupPath.parent_path());
        fs::copy_file(fullPath, backupPath, fs::copy_options::overwrite_existing);
    }
}

void InternalMCPClient::ensurePhotonDirs() {
    fs::create_directories(fs::path(rootPath) / ".photon" / "backups");
}

std::string InternalMCPClient::executeCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "Error: popen() failed!";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
