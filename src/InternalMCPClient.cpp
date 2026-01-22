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

    return {{"result", {{"tools", tools}}}};
}

nlohmann::json InternalMCPClient::callTool(const std::string& name, const nlohmann::json& arguments) {
    if (name == "file_search") {
        return fileSearch(arguments);
    } else if (name == "file_read") {
        return fileRead(arguments);
    } else if (name == "file_write") {
        return fileWrite(arguments);
    } else if (name == "python_sandbox") {
        return pythonSandbox(arguments);
    } else if (name == "bash_execute") {
        return bashExecute(arguments);
    } else if (name == "code_ast_analyze") {
        return codeAstAnalyze(arguments);
    } else if (name == "git_operations") {
        return gitOperations(arguments);
    } else if (name == "web_fetch") {
        return webFetch(arguments);
    } else if (name == "web_search") {
        return webSearch(arguments);
    } else if (name == "harmony_search") {
        return harmonySearch(arguments);
    }
    return {{"error", "Tool not found"}};
}

nlohmann::json InternalMCPClient::fileSearch(const nlohmann::json& args) {
    std::string query = args["query"];
    std::vector<std::string> matches;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
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

    std::string output = executeCommand("python3 " + tmpFile + " 2>&1");
    
    // Clean up
    fs::remove(tmpFile);

    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::bashExecute(const nlohmann::json& args) {
    std::string command = args["command"];
    std::string output = executeCommand(command + " 2>&1");
    return {{"content", {{{"type", "text"}, {"text", output}}}}};
}

nlohmann::json InternalMCPClient::codeAstAnalyze(const nlohmann::json& args) {
    std::string relPath = args["path"];
    fs::path fullPath = fs::path(rootPath) / relPath;

    if (!fs::exists(fullPath)) return {{"error", "File not found"}};

    std::ifstream file(fullPath);
    std::string line;
    std::string result = "AST Analysis for " + relPath + ":\n";

    // Simple regex for C++ and Python
    std::regex cppClass(R"(class\s+([A-Za-z0-9_]+))");
    std::regex cppFunc(R"(([A-Za-z0-9_:]+)\s+([A-Za-z0-9_]+)\s*\([^)]*\)\s*\{?)");
    std::regex pyDef(R"(def\s+([A-Za-z0-9_]+)\s*\()");
    std::regex pyClass(R"(class\s+([A-Za-z0-9_]+)\s*[:\(])");

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, cppClass) || std::regex_search(line, match, pyClass)) {
            result += "[Class] " + match[1].str() + "\n";
        } else if (std::regex_search(line, match, cppFunc) || std::regex_search(line, match, pyDef)) {
            result += "  [Function] " + match[1].str() + (match.size() > 2 ? " " + match[2].str() : "") + "\n";
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
            // Return sanitized and truncated body to avoid token overflow and UTF-8 errors
            std::string body = sanitizeUtf8(res->body, 5000);
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
    
    // DuckDuckGo HTML version is very crawler-friendly
    std::string host = "html.duckduckgo.com";
    std::string path = "/html/?q=" + query; 

    try {
        httplib::Client cli(host);
        cli.set_follow_location(true);
        
        auto res = cli.Get(path.c_str());
        if (!res || res->status != 200) {
            return {{"error", "Failed to reach DuckDuckGo"}};
        }

        std::string html = res->body;
        std::string results;
        
        // Use standard string literal with escapes to avoid raw string issues
        std::regex link_regex("result__a\"\\s+href=\"([^\"]+)\">([^<]+)</a>");
        auto words_begin = std::sregex_iterator(html.begin(), html.end(), link_regex);
        auto words_end = std::sregex_iterator();

        int count = 0;
        for (std::sregex_iterator i = words_begin; i != words_end && count < 5; ++i) {
            std::smatch match = *i;
            results += "Title: " + sanitizeUtf8(match[2].str(), 1000) + "\n";
            results += "Link: " + match[1].str() + "\n\n";
            count++;
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
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d%H%M%S");
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
            {"length", 12}
        }}
    };

    try {
        httplib::Client cli("https://svc-drcn.developer.huawei.com");
        cli.set_follow_location(true);
        
        auto res = cli.Post("/community/servlet/consumer/partnerCommunityService/developer/search", 
                           body.dump(), "application/json");

        if (res && res->status == 200) {
            auto jsonRes = nlohmann::json::parse(res->body);
            std::string summary = "HarmonyOS Search Results for '" + query + "':\n\n";
            
            if (jsonRes.contains("data") && jsonRes["data"].contains("list") && jsonRes["data"]["list"].is_array()) {
                for (const auto& item : jsonRes["data"]["list"]) {
                    std::string title = item.value("name", ""); // API often uses 'name' for title
                    if (title.empty()) title = item.value("title", "No Title");
                    
                    title = sanitizeUtf8(title, 500);
                    
                    std::string link = item.value("url", "");
                    if (!link.empty() && link[0] == '/') {
                        link = "https://developer.huawei.com" + link;
                    }
                    
                    summary += "- Title: " + title + "\n";
                    summary += "  Link: " + link + "\n\n";
                }
            } else {
                summary += "No results found or unexpected response format.";
            }
            return {{"content", {{{"type", "text"}, {"text", summary}}}}};
        } else {
            return {{"error", "Failed to fetch HarmonyOS results: " + (res ? std::to_string(res->status) : "Unknown error")}};
        }
    } catch (const std::exception& e) {
        return {{"error", e.what()}};
    }
}

std::string InternalMCPClient::executeCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return "Error: popen() failed!";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
