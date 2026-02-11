#include <thread>
#include <chrono>
#include "core/UIManager.h"
#include <queue>
#include <condition_variable>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <iomanip>
#include <regex>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#ifdef _WIN32
    #define NOMINMAX  // prevent windows.h from defining min/max macros (breaks std::min/std::max)
    #include <winsock2.h>
#endif
// FileManager.h 已删除,功能由 CoreTools 提供
#include "core/LLMClient.h"
#include "core/ContextManager.h"
#include "core/ConfigManager.h"
#include "mcp/MCPClient.h"
#include "mcp/MCPManager.h"
#include "analysis/LSPClient.h"
#include "utils/SkillManager.h"
#include "analysis/SymbolManager.h"
#include "utils/Logger.h"
#include "analysis/providers/RegexSymbolProvider.h"
#include "analysis/providers/TreeSitterSymbolProvider.h"
// 新架构: Tools 层
#include "tools/ToolRegistry.h"
#include "tools/CoreTools.h"
// Agent 层: Constitution 校验
#include "agent/ConstitutionValidator.h"
#ifdef PHOTON_ENABLE_TREESITTER
#include "tree-sitter-cpp.h"
#include "tree_sitter/tree-sitter-python.h"
#include "tree-sitter-typescript.h"
#include "tree_sitter/tree-sitter-arkts.h"
#endif
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[38;5;196m";
const std::string GREEN = "\033[38;5;46m";
const std::string YELLOW = "\033[38;5;226m";
const std::string BLUE = "\033[38;5;33m";
const std::string MAGENTA = "\033[38;5;201m";
const std::string CYAN = "\033[38;5;51m";
const std::string ITALIC = "\033[3m";
const std::string PURPLE = "\033[38;5;141m";
const std::string ORANGE = "\033[38;5;208m";
const std::string GRAY = "\033[38;5;242m";
const std::string GOLD = "\033[38;5;220m";

// 全局变量用于存储 Git 状态
bool g_hasGit = false;

// Windows兼容的Git检测函数
bool checkGitAvailable() {
#ifdef _WIN32
    // Windows: 首先检查git命令是否存在
    std::string gitPath = findExecutableInPath({"git"});
    if (gitPath.empty()) {
        std::cout << YELLOW << "  ⚠ Git command not found in PATH" << RESET << std::endl;
        std::cout << GRAY << "    - Run 'where git' to check Git location" << RESET << std::endl;
        std::cout << GRAY << "    - Make sure Git is installed and added to PATH" << RESET << std::endl;
        return false;
    }
    
    std::cout << GRAY << "  Git found at: " << gitPath << RESET << std::endl;
    
    // 检查是否在git仓库中
    int result = system("git rev-parse --is-inside-work-tree >nul 2>nul");
    if (result != 0) {
        std::cout << YELLOW << "  ⚠ Git command found but not in a Git repository" << RESET << std::endl;
        std::cout << GRAY << "    - Current directory: " << fs::current_path().u8string() << RESET << std::endl;
        std::cout << GRAY << "    - Run 'git init' to initialize a repository" << RESET << std::endl;
        return false;
    }
    
    return true;
#else
    // Unix/Linux/macOS: 使用原始命令
    return (system("git rev-parse --is-inside-work-tree >/dev/null 2>&1") == 0);
#endif
}

std::string findExecutableInPath(const std::vector<std::string>& names) {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";
    std::string paths = pathEnv;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    size_t start = 0;
    while (start <= paths.size()) {
        size_t end = paths.find(sep, start);
        std::string dir = (end == std::string::npos) ? paths.substr(start) : paths.substr(start, end - start);
        if (!dir.empty()) {
            for (const auto& name : names) {
                fs::path candidate = fs::path(dir) / name;
                if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
                    return candidate.u8string();
                }
#ifdef _WIN32
                // Also try common Windows script extensions
                for (const std::string& ext : {".exe", ".cmd", ".bat"}) {
                    fs::path script = candidate;
                    script += ext;
                    if (fs::exists(script) && fs::is_regular_file(script)) {
                        return script.u8string();
                    }
                }
#endif
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return "";
}

std::vector<std::string> guessExtensionsForCommand(const std::string& command) {
    std::string lower = command;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.find("clangd") != std::string::npos) {
        return {".c", ".cpp", ".h", ".hpp"};
    }
    if (lower.find("pyright") != std::string::npos || lower.find("pylsp") != std::string::npos) {
        return {".py"};
    }
    if (lower.find("typescript-language-server") != std::string::npos) {
        return {".ts", ".tsx", ".js", ".jsx"};
    }
    if (lower.find("arkts") != std::string::npos || lower.find("ets2panda") != std::string::npos) {
        return {".ets"};
    }
    return {};
}

static std::string readTextFileTruncated(const fs::path& filePath, size_t maxBytes) {
    try {
        std::ifstream f(filePath);
        if (!f.is_open()) return "";
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (content.size() > maxBytes) {
            content.resize(maxBytes);
            content += "\n\n…(truncated)…\n";
        }
        return content;
    } catch (...) {
        return "";
    }
}

std::string guessNameForCommand(const std::string& command) {
    std::string lower = command;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    
    if (lower.find("clangd") != std::string::npos) return "Clangd";
    if (lower.find("pyright") != std::string::npos) return "Pyright";
    if (lower.find("pylsp") != std::string::npos) return "Python-LSP";
    if (lower.find("rust-analyzer") != std::string::npos) return "Rust-Analyzer";
    if (lower.find("gopls") != std::string::npos) return "Gopls";
    if (lower.find("typescript-language-server") != std::string::npos) return "TypeScript-LSP";
    if (lower.find("arkts") != std::string::npos || lower.find("ets2panda") != std::string::npos) return "ArkTS-LSP";
    
    // Fallback: use the executable name
    fs::path p(command);
    std::string stem = p.stem().string();
    if (!stem.empty()) {
        stem[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(stem[0])));
        return stem;
    }
    return "Unknown-LSP";
}

std::vector<Config::Agent::LSPServer> autoDetectLspServers() {
    std::vector<Config::Agent::LSPServer> detected;
    std::unordered_set<std::string> added;

    auto addServer = [&](const std::string& name,
                         const std::string& command,
                         const std::vector<std::string>& extensions) {
        if (command.empty() || extensions.empty()) return;
        if (added.insert(command).second) {
            Config::Agent::LSPServer server;
            server.name = name;
            server.command = command;
            server.extensions = extensions;
            detected.push_back(std::move(server));
        }
    };

    if (!findExecutableInPath({"clangd"}).empty()) {
        addServer("Clangd", "clangd", {".c", ".cpp", ".h", ".hpp"});
    }

    if (!findExecutableInPath({"pyright-langserver"}).empty()) {
        addServer("Pyright", "pyright-langserver --stdio", {".py"});
    }
    if (!findExecutableInPath({"pylsp"}).empty()) {
        addServer("Python-LSP", "pylsp", {".py"});
    }

    if (!findExecutableInPath({"typescript-language-server"}).empty()) {
        addServer("TypeScript-LSP", "typescript-language-server --stdio", {".ts", ".tsx", ".js", ".jsx"});
    }

    if (!findExecutableInPath({"gopls"}).empty()) {
        addServer("Go-LSP", "gopls", {".go"});
    }

    if (!findExecutableInPath({"rust-analyzer"}).empty()) {
        addServer("Rust-LSP", "rust-analyzer", {".rs"});
    }

    if (!findExecutableInPath({"jdtls"}).empty()) {
        addServer("Java-LSP", "jdtls", {".java"});
    }

    if (!findExecutableInPath({"bash-language-server"}).empty()) {
        addServer("Bash-LSP", "bash-language-server start", {".sh", ".bash"});
    }

    if (!findExecutableInPath({"cmake-language-server"}).empty()) {
        addServer("CMake-LSP", "cmake-language-server", {".cmake", "CMakeLists.txt"});
    }

    if (!findExecutableInPath({"arkts-lsp-server"}).empty()) {
        addServer("ArkTS-LSP", "arkts-lsp-server --stdio", {".ets"});
    }
    if (!findExecutableInPath({"ets2panda"}).empty()) {
        addServer("ArkTS-LSP", "ets2panda --lsp", {".ets"});
    }

    return detected;
}

bool isRiskyTool(const std::string& toolName) {
    static const std::unordered_set<std::string> risky = {
        "write", "file_write", "file_create", "file_edit_lines", "edit_batch_lines", 
        "bash_execute", "git_operations", "python_sandbox", "pip_install", "schedule"
    };
    return risky.count(toolName) > 0;
}

static std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

bool isBashReadCommand(const std::string& cmd) {
    std::string lower = toLowerCopy(cmd);
    // Block common file-reading commands to prevent tool fallback loops.
    const std::vector<std::string> readTokens = {
        "cat ", "head ", "tail ", "sed -n", "awk ", "grep ", "rg ", "less ", "more ", "nl ", "bat "
    };
    for (const auto& token : readTokens) {
        if (lower.find(token) != std::string::npos) return true;
    }
    return false;
}

void showGitDiff(const std::string& path, const std::string& newContent, bool forceFull = false) {
    // 冲突检测：检查文件自上次读取后是否被外部修改
    // 在 UI 层面提供额外警告
    bool conflict = false;
    if (fs::exists(path)) {
        // 简单通过 git diff 的输出来判断是否有未追踪的外部修改
        // 或者如果 g_hasGit 为真，git diff 本身就会展示出工作区的变化
    }

    std::string originalPath = path;
    bool isNewFile = !fs::exists(path);
    std::string emptyFile = "";
    
    if (isNewFile) {
        // Create an empty file to compare against for new files
        fs::path absolutePath = fs::absolute(fs::u8path(path));
        emptyFile = (absolutePath / ".photon.empty").u8string();
        std::ofstream out(emptyFile);
        out.close();
        originalPath = emptyFile;
    }

    fs::path absolutePath = fs::absolute(fs::u8path(path));
    std::string tmpPath = (absolutePath / ".photon.tmp").u8string();
    std::ofstream tmpFile(tmpPath);
    if (!tmpFile.is_open()) {
        if (!emptyFile.empty()) fs::remove(emptyFile);
        return;
    }
    tmpFile << newContent;
    tmpFile.close();

    bool diffShown = false;
    if (g_hasGit) {
        std::string cmd = "git diff --no-index --color=always \"" + originalPath + "\" \"" + tmpPath + "\" 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            std::string diff;
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                diff += buffer;
            }
            if (pclose(pipe) == 0 || !diff.empty()) {
                // If it's a new file, the diff might contain the temp filename, replace it
                if (isNewFile) {
                    size_t pos = 0;
                    while ((pos = diff.find(emptyFile, pos)) != std::string::npos) {
                        diff.replace(pos, emptyFile.length(), "/dev/null");
                        pos += 9;
                    }
                }
                UIManager::getInstance().setDiff(diff);
                diffShown = true;
            }
        }
    }

    if (!diffShown) {
        std::string simpleDiff = YELLOW + "--- " + (isNewFile ? "/dev/null" : path) + " (Original)\n" + 
                                 GREEN + "+++ " + path + " (New)\n" + RESET +
                                 (isNewFile ? "(New file creation)\n\n" : "(Git not available, showing full new content preview)\n\n") + newContent;
        UIManager::getInstance().setDiff(simpleDiff);
    }

    // 如果是高风险路径且有潜在冲突，输出警告
    if (path.find("src/") != std::string::npos) {
        // 提示用户检查是否有外部修改
        std::cout << YELLOW << "  ℹ Tip: If you've manually edited this file recently, ensure the diff above doesn't overwrite your changes." << RESET << std::endl;
    }

    if (fs::exists(tmpPath)) std::remove(tmpPath.c_str());
    if (!emptyFile.empty() && fs::exists(emptyFile)) std::remove(emptyFile.c_str());
}

std::string renderMarkdown(const std::string& input) {
    std::string output;
    std::string remaining = input;
    
    // 1. Code Blocks (MUST be processed carefully)
    // We'll replace code blocks with placeholders to avoid line-by-line processing
    std::vector<std::string> codeBlocks;
    std::regex code_block_regex(R"(```([a-z]*)\s*([\s\S]*?)\s*```)");
    auto words_begin = std::sregex_iterator(remaining.begin(), remaining.end(), code_block_regex);
    auto words_end = std::sregex_iterator();

    std::string placeholder_prefix = "___CODE_BLOCK_";
    size_t last_pos = 0;
    std::string text_with_placeholders;
    int block_idx = 0;

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        text_with_placeholders += remaining.substr(last_pos, match.position() - last_pos);
        
        std::string lang = match[1].str();
        std::string code = match[2].str();
        std::string rendered = YELLOW + "╭────────── " + (lang.empty() ? "code" : lang) + " ──────────\n" + code + "\n╰────────────────────────" + RESET;
        
        codeBlocks.push_back(rendered);
        text_with_placeholders += placeholder_prefix + std::to_string(block_idx++) + "___";
        last_pos = match.position() + match.length();
    }
    text_with_placeholders += remaining.substr(last_pos);

    // 2. Process line by line for block elements
    std::stringstream ss(text_with_placeholders);
    std::string line;
    std::string processed_text;
    while (std::getline(ss, line)) {
        // Horizontal Rules
        if (std::regex_match(line, std::regex(R"(^---$)"))) {
            line = CYAN + "──────────────────────────────────────────────────" + RESET;
        }
        // Headers
        else if (std::regex_match(line, std::regex(R"(^# (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^# (.*))"), BOLD + MAGENTA + "█ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^## (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^## (.*))"), BOLD + CYAN + "▓ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^### (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^### (.*))"), BOLD + BLUE + "▒ $1" + RESET);
        }
        else if (std::regex_match(line, std::regex(R"(^#### (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^#### (.*))"), BOLD + YELLOW + "░ $1" + RESET);
        }
        // Blockquotes
        else if (std::regex_match(line, std::regex(R"(^> (.*))"))) {
            line = std::regex_replace(line, std::regex(R"(^> (.*))"), BLUE + "┃ " + RESET + ITALIC + "$1" + RESET);
        }
        // Lists
        else if (std::regex_search(line, std::regex(R"(^[\s]*- )"))) {
            line = std::regex_replace(line, std::regex(R"(^([\s]*)- )"), "$1" + CYAN + "•" + RESET + " ");
        }
        else if (std::regex_search(line, std::regex(R"(^[\s]*\d+\. )"))) {
            line = std::regex_replace(line, std::regex(R"(^([\s]*\d+\. ))"), CYAN + "$1" + RESET);
        }
        // Tables (Simple check)
        else if (std::regex_match(line, std::regex(R"(^\|.*\|$)"))) {
            if (std::regex_match(line, std::regex(R"(^\|[-:| ]+\|.*$)"))) {
                line = CYAN + line + RESET;
            } else {
                // Style table pipes
                line = std::regex_replace(line, std::regex(R"(\| )"), CYAN + "┃ " + RESET);
                line = std::regex_replace(line, std::regex(R"( \|)"), " " + CYAN + "┃" + RESET);
                // Also handle the start/end pipes
                if (!line.empty() && line[0] == '|') line = CYAN + "┃" + RESET + line.substr(1);
                if (!line.empty() && line.back() == '|') line = line.substr(0, line.size()-1) + CYAN + "┃" + RESET;
            }
        }

        processed_text += line + "\n";
    }

    // 3. Inline Elements (Bold, Italic, Code, Links)
    output = processed_text;
    // Trim trailing newline
    if (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    output = std::regex_replace(output, std::regex(R"(\*\*\*(.*?)\*\*\*)"), BOLD + ITALIC + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*\*(.*?)\*\*)"), BOLD + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(\*(.*?)\*)"), ITALIC + "$1" + RESET);
    output = std::regex_replace(output, std::regex(R"(`([^`]+)`)"), GREEN + " $1 " + RESET);
    output = std::regex_replace(output, std::regex(R"(\[(.*?)\]\((.*?)\))"), BLUE + "$1" + RESET + " (" + CYAN + "$2" + RESET + ")");

    // 4. Restore Code Blocks
    for (int i = 0; i < block_idx; ++i) {
        std::string placeholder = placeholder_prefix + std::to_string(i) + "___";
        size_t pos = output.find(placeholder);
        if (pos != std::string::npos) {
            output.replace(pos, placeholder.length(), codeBlocks[i]);
        }
    }

    return output;
}

void printLogo() {
    std::string frame = CYAN + "  ====================================================================" + RESET;
    std::vector<std::string> lines = {
        CYAN + BOLD + R"(         ____    __  __  ____   ______  ____    _   __)" + RESET,
        CYAN + BOLD + R"(        / __ \  / / / / / __ \ /_  __/ / __ \  / | / /)" + RESET,
        CYAN + BOLD + R"(       / /_/ / / /_/ / / / / /  / /   / / / / /  |/ / )" + RESET,
        CYAN + BOLD + R"(      / ____/ / __  / / /_/ /  / /   / /_/ / / /|  /  )" + RESET,
        CYAN + BOLD + R"(     /_/     /_/ /_/  \____/  /_/    \____/ /_/ |_/   )" + RESET
    };

    std::cout << "\n" << frame << std::endl;
    for (const auto& line : lines) {
        std::cout << line << std::endl;
    }
    std::cout << frame << std::endl;
    std::cout << GRAY << "        ─── " << ITALIC << "The Native Agentic Core v1.0" << RESET << GRAY << " ───\n" << std::endl;
}

void printUsage() {
    std::cout << "Usage: photon <directory_path> [config_path]" << std::endl;
}

nlohmann::json formatToolsForLLM(const nlohmann::json& mcpTools) {
    nlohmann::json llmTools = nlohmann::json::array();
    for (const auto& t : mcpTools) {
        nlohmann::json tool;
        tool["type"] = "function";
        nlohmann::json function;
        // Combine server name and tool name to ensure uniqueness and allow dispatching
        function["name"] = t["server_name"].get<std::string>() + "__" + t["name"].get<std::string>();
        function["description"] = t["description"];
        function["parameters"] = t["inputSchema"];
        tool["function"] = function;
        llmTools.push_back(tool);
    }
    return llmTools;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Force UTF-8 encoding for Windows terminal
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 设置Git命令环境
    _putenv("GIT_TERMINAL_PROMPT=0");  // 禁用Git交互式提示

    // Enable Virtual Terminal Processing for ANSI colors on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }
#endif

    // Initialize UIManager
    // Default mode is CLI
    UIManager::getInstance().setMode(UIManager::Mode::CLI);

    // 全局 Git 状态检测
    g_hasGit = checkGitAvailable();
    if (g_hasGit) {
        std::cout << GRAY << "  (Git environment detected, using Git for version control)" << RESET << std::endl;
    } else {
        std::cout << GRAY << "  (No Git detected, using local backups for version control)" << RESET << std::endl;
    }

    // Global exception handler to prevent silent crash
    try {
        printLogo(); // 恢复 Logo 展示

        std::string path = ".";
        
        // Determine executable directory for default config search
        fs::path exeDir;
        fs::path photonDir = fs::u8path(".photon");
        if (!fs::exists(photonDir)) {
            fs::create_directories(photonDir);
        }
        
        try {
#ifdef _WIN32
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            exeDir = fs::path(szPath).parent_path();
#elif defined(__APPLE__)
            char szPath[1024];
            uint32_t size = sizeof(szPath);
            if (_NSGetExecutablePath(szPath, &size) == 0) {
                exeDir = fs::canonical(szPath).parent_path();
            }
#else
            exeDir = fs::canonical("/proc/self/exe").parent_path();
#endif
        } catch (...) {}

        std::string configPath = "config.json"; // Default to current directory

        if (argc >= 2) {
            path = argv[1];
        }

        // 与改动前一致：只查 cwd、../、exe 同目录；若传了第二参数则用其作 config 路径
        if (argc >= 3) {
            configPath = argv[2];
        } else {
            if (fs::exists(fs::u8path("config.json"))) {
                configPath = "config.json";
            } else if (fs::exists(fs::u8path("../config.json"))) {
                configPath = "../config.json";
            } else if (argc >= 2 && fs::exists(fs::absolute(fs::u8path(path)) / "config.json")) {
                configPath = (fs::absolute(fs::u8path(path)) / "config.json").u8string();
            } else if (!exeDir.empty() && fs::exists(exeDir / "config.json")) {
                configPath = (exeDir / "config.json").u8string();
            }
        }

    Config cfg;
    try {
        if (!fs::exists(fs::u8path(configPath))) {
            throw std::runtime_error("Configuration file not found: " + configPath);
        }
        cfg = Config::load(configPath);
        cfg.ensurePhotonRules(); // 自动生成或更新 .photonrules
        std::cout << GREEN << "✔ Loaded configuration from: " << BOLD << configPath << RESET << std::endl;
    } catch (const std::exception& e) {
        std::cerr << RED << "✖ Failed to load config: " << e.what() << RESET << std::endl;
        printUsage();
        return 1;
    }

    if (cfg.agent.lspServerPath.empty()) {
        cfg.agent.lspServerPath = findExecutableInPath({
            "clangd", "clangd-18", "clangd-17", "clangd-16", "pyright-langserver"
        });
    }

    auto llmClient = std::make_shared<LLMClient>(cfg.llm.apiKey, cfg.llm.baseUrl, cfg.llm.model);
    ContextManager contextManager(llmClient, cfg.agent.contextThreshold);

    // Initialize MCP Manager and connect all servers
    MCPManager mcpManager;
    auto mcpFuture = std::async(std::launch::async, [&]() {
        if (cfg.agent.useBuiltinTools) {
            mcpManager.initBuiltin(path, cfg.agent.searchApiKey);
        }
        return mcpManager.initFromConfig(cfg.mcpServers);
    });

    // Initialize Skill Manager (Async)
    SkillManager skillManager;
    auto skillFuture = std::async(std::launch::async, [&]() {
        // 1. Load bundled internal skills
        try {
            fs::path exeDir;
#ifdef _WIN32
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileNameW(NULL, szPath, MAX_PATH) != 0) exeDir = fs::path(szPath).parent_path();
#elif defined(__APPLE__)
            char szPath[4096];
            uint32_t size = sizeof(szPath);
            if (_NSGetExecutablePath(szPath, &size) == 0) exeDir = fs::canonical(szPath).parent_path();
#else
            exeDir = fs::canonical("/proc/self/exe").parent_path();
#endif
            fs::path builtinPath;
            if (!exeDir.empty() && fs::exists(exeDir / "builtin_skills")) builtinPath = exeDir / "builtin_skills";
            else if (fs::exists(fs::path("builtin_skills"))) builtinPath = fs::path("builtin_skills");

            if (!builtinPath.empty() && fs::is_directory(builtinPath)) {
                skillManager.loadFromRoot(builtinPath.u8string(), true);
            }
        } catch (...) {}

        // 2. Sync & Load specialized skills
        fs::path globalDataPath;
        try {
            fs::path exeDir;
#ifdef _WIN32
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileNameW(NULL, szPath, MAX_PATH) != 0) exeDir = fs::path(szPath).parent_path();
#elif defined(__APPLE__)
            char szPath[4096];
            uint32_t size = sizeof(szPath);
            if (_NSGetExecutablePath(szPath, &size) == 0) exeDir = fs::canonical(szPath).parent_path();
#else
            exeDir = fs::canonical("/proc/self/exe").parent_path();
#endif
            globalDataPath = exeDir / ".photon";
        } catch (...) {
            // Ensure we use absolute path for the project .photon directory
            fs::path absolutePath = fs::absolute(fs::u8path(path));
            globalDataPath = absolutePath / ".photon";
        }
        skillManager.syncAndLoad(cfg.agent.skillRoots, globalDataPath.u8string());
    });
    
    // Initialize Symbol Manager and start async scan
    // Ensure path is absolute so symbols are generated in the correct directory
    fs::path absolutePath = fs::absolute(fs::u8path(path));
    SymbolManager symbolManager(absolutePath.u8string());
    symbolManager.setFallbackOnEmpty(cfg.agent.symbolFallbackOnEmpty);
    
    // 设置符号扫描忽略模式
    if (!cfg.agent.symbolIgnorePatterns.empty()) {
        symbolManager.setIgnorePatterns(cfg.agent.symbolIgnorePatterns);
    }
    
#ifdef PHOTON_ENABLE_TREESITTER
    if (cfg.agent.enableTreeSitter) {
        auto treeProvider = std::make_unique<TreeSitterSymbolProvider>();
        treeProvider->registerLanguage("cpp", {".cpp", ".h", ".hpp"}, tree_sitter_cpp());
        treeProvider->registerLanguage("python", {".py"}, tree_sitter_python());
        treeProvider->registerLanguage("typescript", {".ts", ".tsx"}, tree_sitter_typescript());
        treeProvider->registerLanguage("arkts", {".ets"}, tree_sitter_arkts());
        for (const auto& lang : cfg.agent.treeSitterLanguages) {
            treeProvider->registerLanguageFromLibrary(lang.name, lang.extensions, lang.libraryPath, lang.symbol);
        }
        symbolManager.registerProvider(std::move(treeProvider));
    }
#endif
    symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
    
    // 启动时阻塞确保符号索引可用：优先复用磁盘缓存，仅当文件有变化才重建
    bool upToDate = symbolManager.isIndexUpToDate();
    if (!upToDate) {
        std::cout << "[Init] Building symbol index..." << std::endl;
        symbolManager.scanBlocking();
    } else if (cfg.agent.enableDebug) {
        std::cout << "[Init] Symbol index cache is up-to-date, skipping rebuild" << std::endl;
    }
    std::cout << "[Init] Symbol index ready: " << symbolManager.getSymbolCount() << " symbols" << std::endl;
    
    // 启动文件监听
    symbolManager.startWatching(5);

    // Initialize LSP clients (Parallel initialization)
    std::vector<std::unique_ptr<LSPClient>> lspClients;
    std::vector<Config::Agent::LSPServer> availableLspServers;
    std::unordered_map<std::string, LSPClient*> lspByExt;
    LSPClient* lspFallback = nullptr;
    auto rootUri = cfg.agent.lspRootUri;
    if (rootUri.empty()) {
        rootUri = "file://" + fs::absolute(fs::u8path(path)).u8string();
    }

    struct LspInitResult {
        std::vector<std::unique_ptr<LSPClient>> clients;
        std::vector<Config::Agent::LSPServer> availableServers;
    };

    auto lspInitFuture = std::async(std::launch::async, [&]() -> LspInitResult {
        LspInitResult result;
        
        // 如果 LSP 被禁用，直接返回空结果
        if (!cfg.agent.enableLSP) {
            return result;
        }
        
        std::vector<Config::Agent::LSPServer> merged = cfg.agent.lspServers;
        std::unordered_set<std::string> seen;
        for (const auto& s : merged) seen.insert(s.command);

        if (!cfg.agent.lspServerPath.empty()) {
            Config::Agent::LSPServer server;
            server.command = cfg.agent.lspServerPath;
            server.name = guessNameForCommand(server.command);
            server.extensions = guessExtensionsForCommand(server.command);
            if (!server.command.empty() && seen.insert(server.command).second) {
                merged.push_back(std::move(server));
            }
        }

        for (auto& s : autoDetectLspServers()) {
            if (s.command.empty()) continue;
            if (seen.insert(s.command).second) {
                merged.push_back(std::move(s));
            }
        }

        cfg.agent.lspServers = merged;

        struct LspInitItem {
            Config::Agent::LSPServer server;
            std::unique_ptr<LSPClient> client;
            bool ok = false;
        };

        std::vector<std::future<LspInitItem>> lspFutures;
        for (const auto& server : cfg.agent.lspServers) {
            if (server.command.empty()) continue;
            lspFutures.push_back(std::async(std::launch::async, [server, rootUri]() {
                LspInitItem item;
                item.server = server;
                auto client = std::make_unique<LSPClient>(server.command, rootUri);
                if (client->initialize()) {
                    item.client = std::move(client);
                    item.ok = true;
                }
                return item;
            }));
        }

        for (auto& f : lspFutures) {
            auto item = f.get();
            if (item.ok) {
                result.availableServers.push_back(item.server);
                result.clients.push_back(std::move(item.client));
            }
        }
        return result;
    });

    // ============================================================
    // 新架构: ToolRegistry 初始化
    // ============================================================
    ToolRegistry toolRegistry;
    
    // 注册核心工具
    std::cout << CYAN << "  → Registering core tools..." << RESET << std::endl;
    toolRegistry.registerTool(std::make_unique<ReadCodeBlockTool>(path, &symbolManager, cfg.agent.enableDebug));
    toolRegistry.registerTool(std::make_unique<ApplyPatchTool>(path, g_hasGit));  // 统一补丁工具：只接受 unified diff
    toolRegistry.registerTool(std::make_unique<RunCommandTool>(path));
    toolRegistry.registerTool(std::make_unique<ListProjectFilesTool>(path));
    
    std::cout << GREEN << "  ✔ Registered " << toolRegistry.getToolCount() << " core tools" << RESET << std::endl;

    // 获取工具的 Schema (给 LLM 使用)
    auto toolSchemas = toolRegistry.listToolSchemas();
    nlohmann::json llmTools = nlohmann::json::array();
    for (const auto& schema : toolSchemas) {
        llmTools.push_back(schema);
    }

    // 保留外部 MCP 工具 (如果有)
    auto mcpTools = mcpManager.getAllTools();
    for (const auto& mcpTool : mcpTools) {
        // 排除已被新工具替代的旧工具
        std::string toolName = mcpTool.value("name", "");
        if (toolName != "read" && toolName != "write" && 
            toolName != "file_read" && toolName != "file_write" &&
            toolName != "bash_execute" && toolName != "list_dir_tree") {
            // 转换 MCP 工具格式
            nlohmann::json tool;
            tool["type"] = "function";
            nlohmann::json function;
            function["name"] = mcpTool["server_name"].get<std::string>() + "__" + mcpTool["name"].get<std::string>();
            function["description"] = mcpTool["description"];
            function["parameters"] = mcpTool["inputSchema"];
            tool["function"] = function;
            llmTools.push_back(tool);
        }
    }

    std::cout << GREEN << "  ✔ Engine active. Total tools: " << llmTools.size() << RESET << std::endl;

    std::cout << "  " << CYAN << "Model  " << RESET << " : " << PURPLE << cfg.llm.model << RESET << std::endl;
    
    std::cout << "\n  " << YELLOW << "Shortcuts:" << RESET << std::endl;
    std::cout << GRAY << "  ┌──────────────────────────────────────────────────────────┐" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "tools   " << RESET << GRAY << " List all sensors   " 
              << "│ " << RESET << BOLD << "undo    " << RESET << GRAY << " Revert change     " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "patch   " << RESET << GRAY << " Preview last patch " 
              << "│ " << RESET << BOLD << "        " << RESET << GRAY << "                   " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "skills  " << RESET << GRAY << " List active skills " 
              << "│ " << RESET << BOLD << "lsp     " << RESET << GRAY << " List LSP servers  " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "tasks   " << RESET << GRAY << " List sched tasks   " 
              << "│ " << RESET << BOLD << "compress" << RESET << GRAY << " Summary memory    " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "memory  " << RESET << GRAY << " Show long-term mem " 
              << "│ " << RESET << BOLD << "clear   " << RESET << GRAY << " Reset context     " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "exit    " << RESET << GRAY << " Terminate agent    " 
              << "│ " << RESET << BOLD << "        " << RESET << GRAY << "                   " << "│" << RESET << std::endl;
    std::cout << GRAY << "  └──────────────────────────────────────────────────────────┘" << RESET << std::endl;

    // Optimization: Define the core identity as an Autonomous Agent.
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream date_ss;
    struct tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif
    date_ss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");

    // ============================================================
    // Photon Agent Constitution v2.0
    // ============================================================
    // Inject the actual Constitution text (truncated) so the model is explicitly informed.
    std::string constitutionText;
    {
        fs::path docPath = fs::absolute(fs::u8path(path)) / "docs" / "tutorials" / "photon_agent_constitution_v_2.md";
        if (fs::exists(docPath) && fs::is_regular_file(docPath)) {
            constitutionText = readTextFileTruncated(docPath, 20000);
        }
    }

    std::string systemPrompt = 
        "You are Photon.\n"
        "You must operate under Photon Agent Constitution v2.0.\n"
        "All behavior is governed by the constitution and validated configuration.\n\n" +
        (constitutionText.empty() ? std::string("") : (std::string("# Constitution v2.0\n\n") + constitutionText + "\n\n")) +
        
        // Project-specific configuration (from config.json)
        cfg.llm.systemRole + "\n\n" +
        
        // Skills interface (lazy-loaded capabilities)
        skillManager.getSystemPromptAddition() + "\n" +
        
        // Runtime context (minimal, factual)
        "Working directory: " + path + "\n" +
        "Current time: " + date_ss.str() + "\n";
    
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    // Lazy initialization flag
    bool engineInitialized = false;
    std::unordered_set<std::string> recentQueries; // 记录最近的搜索，防止死循环
    std::unordered_map<std::string, std::string> readSummaries;
    std::vector<std::string> readSummaryOrder;
    const size_t maxReadSummaries = 20;
    const std::string readSummaryTag = "[READ_SUMMARY]";

    auto startsWith = [](const std::string& s, const std::string& prefix) {
        return s.rfind(prefix, 0) == 0;
    };
    auto isReadRejection = [&](const std::string& text) {
        return text.find("❌ READ_FAIL") != std::string::npos ||
               text.find("⚠️ READ_EMPTY") != std::string::npos ||
               text.find("⚠️  请先使用 plan") != std::string::npos ||
               text.find("⚠️  读取范围不在检索计划内") != std::string::npos ||
               text.find("⚠️  批量读取包含计划外范围") != std::string::npos ||
               text.find("⚠️  读取范围过大") != std::string::npos ||
               text.find("⚠️  单文件读取预算已达上限") != std::string::npos ||
               text.find("⚠️ 读取目标是目录") != std::string::npos ||
               text.find("⚠️ 读取目标不是普通文件") != std::string::npos ||
               text.find("⚠️ 已阻止仅凭 path 的读取") != std::string::npos;
    };
    auto extractReadText = [](const nlohmann::json& result) -> std::string {
        if (!result.contains("content") || !result["content"].is_array()) return "";
        std::string text;
        for (const auto& item : result["content"]) {
            if (item.contains("type") && item["type"] == "text" && item.contains("text")) {
                if (!text.empty()) text += "\n";
                text += item["text"].get<std::string>();
            }
        }
        return text;
    };
    auto buildReadKey = [](const nlohmann::json& args) -> std::string {
        if (args.contains("requests") && args["requests"].is_array()) {
            std::string key = "batch:";
            int count = 0;
            for (const auto& req : args["requests"]) {
                if (count++ > 0) key += ";";
                std::string path = req.value("path", "");
                int start = req.value("start_line", 0);
                int end = req.value("end_line", 0);
                key += path + ":" + std::to_string(start) + "-" + std::to_string(end);
            }
            return key;
        }
        if (args.contains("mode") && args["mode"].is_object()) {
            std::string type = args["mode"].value("type", "");
            std::string path = args.value("path", "");
            if (type == "symbol") {
                std::string name = args["mode"].value("name", "");
                if (name.empty()) name = args.value("query", "");
                return "symbol:" + name;
            }
            if (type == "range") {
                int start = args["mode"].value("start", args.value("start_line", 0));
                int end = args["mode"].value("end", args.value("end_line", 0));
                return path + ":" + std::to_string(start) + "-" + std::to_string(end);
            }
            if (type == "full") {
                return "full:" + path;
            }
        }
        if (args.contains("query")) {
            return "query:" + args.value("query", "");
        }
        std::string path = args.value("path", "");
        int start = args.value("start_line", 0);
        int end = args.value("end_line", 0);
        if (!path.empty() && (start > 0 || end > 0)) {
            return path + ":" + std::to_string(start) + "-" + std::to_string(end);
        }
        if (!path.empty()) return path;
        return "read";
    };
    auto storeReadSummary = [&](const std::string& key, const std::string& summary) {
        if (key.empty() || summary.empty()) return;
        if (!readSummaries.count(key)) {
            readSummaryOrder.push_back(key);
            if (readSummaryOrder.size() > maxReadSummaries) {
                readSummaries.erase(readSummaryOrder.front());
                readSummaryOrder.erase(readSummaryOrder.begin());
            }
        }
        readSummaries[key] = summary;
    };
    auto injectReadSummaries = [&]() {
        for (size_t i = 0; i < messages.size(); ) {
            if (messages[i].contains("role") && messages[i]["role"] == "system" &&
                messages[i].contains("content") && messages[i]["content"].is_string()) {
                std::string content = messages[i]["content"].get<std::string>();
                if (startsWith(content, readSummaryTag)) {
                    messages.erase(messages.begin() + i);
                    continue;
                }
            }
            ++i;
        }
        if (readSummaryOrder.empty()) return;
        std::string content = readSummaryTag + "\n已读摘要：\n";
        for (const auto& key : readSummaryOrder) {
            auto it = readSummaries.find(key);
            if (it == readSummaries.end()) continue;
            content += "- " + key + ": " + it->second + "\n";
        }
        if (messages.size() >= 1) {
            messages.insert(messages.begin() + 1, {{"role", "system"}, {"content", content}});
        } else {
            messages.push_back({{"role", "system"}, {"content", content}});
        }
    };

    while (true) {
        std::string userInput;
        
        // CLI Mode
        // 获取当前 Git 分支和状态
        std::string gitStatusLine = "";
        if (g_hasGit) {
            char buffer[128];
            std::string branch = "";
#ifdef _WIN32
            FILE* pipe = _popen("git rev-parse --abbrev-ref HEAD 2>nul", "r");
#else
            FILE* pipe = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
#endif
            if (pipe) {
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    branch = buffer;
                    if (!branch.empty() && branch.back() == '\n') branch.pop_back();
                }
#ifdef _WIN32
                _pclose(pipe);
#else
                pclose(pipe);
#endif
            }
            
#ifdef _WIN32
            bool isDirty = (system("git diff --quiet 2>nul") != 0);
#else
            bool isDirty = (system("git diff --quiet 2>/dev/null") != 0);
#endif
            if (!branch.empty()) {
                gitStatusLine = GRAY + "(" + BLUE + branch + (isDirty ? YELLOW + "*" : "") + GRAY + ") " + RESET;
            }
        }

        std::cout << "\n" << gitStatusLine << CYAN << BOLD << "❯ " << RESET << std::flush;
        
        if (!std::getline(std::cin, userInput)) break;
        if (userInput.empty()) continue;

        if (userInput == "exit") break;

        // Reset query tracking for new user input
        recentQueries.clear();

        // Lazy wait for background tasks only when needed
        if (!engineInitialized) {
            std::cout << GRAY << "  (Finishing engine initialization...)" << RESET << "\r" << std::flush;
            mcpFuture.get();
            skillFuture.get();
            auto lspInitResult = lspInitFuture.get();
            
            // Setup LSP mappings
            lspClients = std::move(lspInitResult.clients);
            availableLspServers = std::move(lspInitResult.availableServers);
            for (const auto& server : availableLspServers) {
                for (auto& client : lspClients) {
                    for (auto ext : server.extensions) {
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        lspByExt[ext] = client.get();
                    }
                }
            }
            if (!lspClients.empty()) lspFallback = lspClients[0].get();

            // NOTE: InternalMCPClient 已删除,改用新的 ToolRegistry 架构
            // 旧代码已注释:
            // if (auto* builtin = dynamic_cast<InternalMCPClient*>(mcpManager.getClient("builtin"))) {
            //     builtin->setSkillManager(&skillManager);
            //     builtin->setSymbolManager(&symbolManager);
            //     builtin->setSemanticManager(semanticManager.get());
            //     if (!lspClients.empty()) builtin->setLSPClients(lspByExt, lspFallback);
            // }

            // 设置 LSP 客户端（扫描已经在启动时开始了）
            symbolManager.setLSPClients(lspByExt, lspFallback);
            
            // 清除初始化提示（使用足够长的空格并换行）
            std::cout << "\r" << std::string(60, ' ') << "\r" << std::flush;
            engineInitialized = true;
        }
        
        if (userInput == "clear") {
            messages = nlohmann::json::array();
            messages.push_back({{"role", "system"}, {"content", systemPrompt}});
            readSummaries.clear();
            readSummaryOrder.clear();
            std::cout << GREEN << "✔ Context cleared (Forgotten)." << RESET << std::endl;
            continue;
        }

        if (userInput == "compress") {
            messages = contextManager.forceCompress(messages);
            continue;
        }

        if (userInput == "tools") {
            std::cout << CYAN << "\n--- Available Tools ---" << RESET << std::endl;
            
            // 显示核心工具 (ToolRegistry)
            std::cout << GREEN << BOLD << "\n[Core Tools]" << RESET << GRAY << " (极简原子工具)" << RESET << std::endl;
            auto coreSchemas = toolRegistry.listToolSchemas();
            for (const auto& schema : coreSchemas) {
                if (schema.contains("function")) {
                    auto func = schema["function"];
                    std::string name = func.value("name", "unknown");
                    std::string desc = func.value("description", "No description");
                    std::cout << PURPLE << BOLD << "  • " << name << RESET << std::endl;
                    std::cout << GRAY << "    " << desc << RESET << std::endl;
                }
            }
            
            // 显示 MCP 工具
            if (!mcpTools.empty()) {
                std::cout << BLUE << BOLD << "\n[MCP Tools]" << RESET << GRAY << " (外部工具)" << RESET << std::endl;
                for (const auto& t : mcpTools) {
                    std::string server = t.value("server_name", "unknown");
                    std::string name = t.value("name", "unknown");
                    std::string desc = t.value("description", "No description");
                    
                    std::cout << PURPLE << BOLD << "  • " << name << RESET 
                              << GRAY << " (" << server << ")" << RESET << std::endl;
                    std::cout << GRAY << "    " << desc << RESET << std::endl;
                }
            }
            
            std::cout << CYAN << "\n-----------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "patch") {
            try {
                fs::path absolutePath = fs::absolute(fs::u8path(path));
                fs::path lastPatchPath = absolutePath / ".photon" / "patches" / "last.patch";
                if (!fs::exists(lastPatchPath) || !fs::is_regular_file(lastPatchPath)) {
                    // 回退预览：没有 last.patch 时，预览当前工作区的改动（优先 git diff，否则用本地备份 diff）
                    if (g_hasGit) {
                        std::cout << CYAN << "\n--- Working Tree Diff (no last.patch) ---" << RESET << std::endl;
#ifdef _WIN32
                        system("git diff --stat");
                        system("git diff --color=always");
#else
                        system("git diff --stat 2>/dev/null || true");
                        system("git diff --color=always 2>/dev/null || true");
#endif
                        std::cout << CYAN << "----------------------------------------\n" << RESET << std::endl;
                        continue;
                    }

                    std::string lastFile = mcpManager.getLastModifiedFile("builtin");
                    if (!lastFile.empty()) {
                        fs::path backupDir = absolutePath / ".photon" / "backups";
                        fs::path lf = fs::u8path(lastFile);
                        fs::path backupRel;
                        if (!lf.is_absolute()) {
                            backupRel = lf;
                        } else {
                            fs::path rn = lf.root_name();
                            fs::path rp = lf.relative_path();
                            if (!rn.empty()) {
                                std::string s = rn.u8string();
                                for (auto& ch : s) if (ch == ':' || ch == '\\') ch = '_';
                                backupRel = fs::path("abs") / fs::u8path(s) / rp;
                            } else {
                                backupRel = fs::path("abs") / rp;
                            }
                        }
                        fs::path backupPath = backupDir / backupRel;
                        if (fs::exists(backupPath)) {
                            std::ifstream bFile(backupPath);
                            std::string bContent((std::istreambuf_iterator<char>(bFile)), std::istreambuf_iterator<char>());
                            std::cout << CYAN << "\n--- Last File Diff (no last.patch, via backup) ---" << RESET << std::endl;
                            showGitDiff(lastFile, bContent, true);
                            std::cout << CYAN << "-------------------------------------------------\n" << RESET << std::endl;
                            continue;
                        }
                    }

                    std::cout << YELLOW << "⚠ No last.patch found, and no Git/backups diff available." << RESET << std::endl;
                    continue;
                }

                std::cout << CYAN << "\n--- Last Patch Preview ---" << RESET << std::endl;

                if (g_hasGit) {
#ifdef _WIN32
                    system(("git apply --stat \"" + lastPatchPath.u8string() + "\"").c_str());
#else
                    system(("git apply --stat \"" + lastPatchPath.u8string() + "\" 2>/dev/null || true").c_str());
#endif
                }

                std::string patchText = readTextFileTruncated(lastPatchPath, 20000);
                if (patchText.empty()) {
                    std::cout << GRAY << "  (Patch file is empty)" << RESET << std::endl;
                } else {
                    std::cout << patchText << std::endl;
                }

                std::cout << CYAN << "-------------------------\n" << RESET << std::endl;
            } catch (...) {
                std::cout << RED << "✖ Failed to preview last patch." << RESET << std::endl;
            }
            continue;
        }

        if (userInput == "skills") {
            std::cout << CYAN << "\n--- Loaded Skills ---\n" << RESET << std::endl;
            if (skillManager.getCount() == 0) {
                std::cout << GRAY << "  (No skills loaded)" << RESET << std::endl;
            } else {
                // Group 1: Built-in Skills
                std::cout << GREEN << BOLD << "[Built-in]" << RESET << GRAY << " (核心专家技能)" << RESET << std::endl;
                bool hasBuiltin = false;
                for (const auto& [name, skill] : skillManager.getSkills()) {
                    if (skill.isBuiltin) {
                        std::cout << PURPLE << BOLD << "  • " << name << RESET << std::endl;
                        std::cout << GRAY << "    Source: " << skill.path << RESET << std::endl;
                        hasBuiltin = true;
                    }
                }
                if (!hasBuiltin) std::cout << GRAY << "  (无内置技能)" << RESET << std::endl;

                // Group 2: External Skills
                std::cout << "\n" << BLUE << BOLD << "[External]" << RESET << GRAY << " (自定义/项目技能)" << RESET << std::endl;
                bool hasExternal = false;
                for (const auto& [name, skill] : skillManager.getSkills()) {
                    if (!skill.isBuiltin) {
                        std::cout << PURPLE << BOLD << "  • " << name << RESET << std::endl;
                        std::cout << GRAY << "    Source: " << skill.path << RESET << std::endl;
                        hasExternal = true;
                    }
                }
                if (!hasExternal) std::cout << GRAY << "  (无外置技能)" << RESET << std::endl;
            }
            std::cout << CYAN << "\n---------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "lsp") {
            std::cout << CYAN << "\n--- Available LSP Servers ---" << RESET << std::endl;
            if (availableLspServers.empty()) {
                std::cout << GRAY << "  (No LSP servers available)" << RESET << std::endl;
            } else {
                for (const auto& server : availableLspServers) {
                    std::cout << GREEN << BOLD << "  • " << server.name << RESET << std::endl;
                    std::cout << GRAY << "    Command: " << server.command << RESET << std::endl;
                    if (!server.extensions.empty()) {
                        std::string exts;
                        for (size_t i = 0; i < server.extensions.size(); ++i) {
                            exts += server.extensions[i];
                            if (i + 1 < server.extensions.size()) exts += ", ";
                        }
                        std::cout << GRAY << "    Extensions: " << exts << RESET << std::endl;
                    }
                }
            }
            std::cout << CYAN << "----------------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "tasks") {
            std::cout << CYAN << "\n--- Active Scheduled Tasks ---" << RESET << std::endl;
            nlohmann::json res = mcpManager.callTool("builtin", "tasks", {});
            if (res.contains("content") && res["content"][0].contains("text")) {
                std::cout << res["content"][0]["text"].get<std::string>() << std::endl;
            } else {
                std::cout << "Failed to retrieve tasks." << std::endl;
            }
            std::cout << CYAN << "------------------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "memory") {
            std::cout << CYAN << "\n--- Long-term Memory ---" << RESET << std::endl;
            nlohmann::json res = mcpManager.callTool("builtin", "memory", {{"action", "query"}});
            if (res.contains("content") && res["content"][0].contains("text")) {
                std::cout << renderMarkdown(res["content"][0]["text"].get<std::string>()) << std::endl;
            } else {
                std::cout << "Failed to retrieve memory list." << std::endl;
            }
            std::cout << CYAN << "------------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "undo") {
            // 优先：撤销最近一次 apply_patch 的 unified diff（如果存在）
            try {
                fs::path absolutePath = fs::absolute(fs::u8path(path));
                fs::path patchDir = absolutePath / ".photon" / "patches";
                fs::path stackPath = patchDir / "patch_stack.json";
                fs::path patchPath = patchDir / "last.patch";

                // 如果有栈，优先用栈顶 patch（支持多次 undo）
                if (fs::exists(stackPath) && fs::is_regular_file(stackPath)) {
                    try {
                        std::ifstream sf(stackPath);
                        nlohmann::json stack;
                        sf >> stack;
                        if (stack.is_array() && !stack.empty()) {
                            auto& last = stack.back();
                            if (last.contains("patch_path") && last["patch_path"].is_string()) {
                                patchPath = fs::u8path(last["patch_path"].get<std::string>());
                            }
                        }
                    } catch (...) {}
                }

                if (g_hasGit && fs::exists(patchPath) && fs::is_regular_file(patchPath)) {
                    std::cout << YELLOW << BOLD << "Reverting last patch: " << RESET << patchPath.u8string() << std::endl;

                    while (true) {
                        std::cout << "\n " << YELLOW << BOLD << "⚠  UNDO PATCH CONFIRMATION" << RESET << std::endl;
                        std::cout << GRAY << "   Action: " << RESET << "git apply -R <patch>" << std::endl;
                        std::cout << "   " << BOLD << "[y]" << RESET << " Yes  "
                                  << BOLD << "[n]" << RESET << " No  "
                                  << BOLD << "[v]" << RESET << " View Patch" << std::endl;
                        std::cout << " " << CYAN << BOLD << "❯ " << RESET << std::flush;

                        std::string confirm;
                        std::getline(std::cin, confirm);
                        std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);

                        if (confirm == "v") {
#ifdef _WIN32
                            system(("git apply --stat \"" + patchPath.u8string() + "\" >nul 2>nul").c_str());
#else
                            system(("git apply --stat \"" + patchPath.u8string() + "\" 2>/dev/null || true").c_str());
#endif
                            continue;
                        }

                        if (confirm == "y" || confirm == "yes") {
#ifdef _WIN32
                            int rc = system(("git apply -R \"" + patchPath.u8string() + "\" >nul 2>nul").c_str());
#else
                            int rc = system(("git apply -R \"" + patchPath.u8string() + "\" 2>/dev/null").c_str());
#endif
                            if (rc == 0) {
                                // 从 patch 栈里弹出（支持多次 undo）
                                std::error_code ec;
                                try {
                                    if (fs::exists(stackPath) && fs::is_regular_file(stackPath)) {
                                        std::ifstream sf(stackPath);
                                        nlohmann::json stack;
                                        sf >> stack;
                                        if (stack.is_array() && !stack.empty()) {
                                            stack.erase(stack.end() - 1);
                                            std::ofstream of(stackPath);
                                            of << stack.dump(2);
                                        }
                                    }
                                } catch (...) {}

                                // 删除刚撤销的 patch 文件（可选）
                                fs::remove(patchPath, ec);

                                // 维护 last.patch 指向新的栈顶（或删除）
                                try {
                                    nlohmann::json stack;
                                    if (fs::exists(stackPath) && fs::is_regular_file(stackPath)) {
                                        std::ifstream sf(stackPath);
                                        sf >> stack;
                                    }
                                    if (stack.is_array() && !stack.empty()) {
                                        auto& last = stack.back();
                                        if (last.contains("patch_path") && last["patch_path"].is_string()) {
                                            fs::path newTop = fs::u8path(last["patch_path"].get<std::string>());
                                            if (fs::exists(newTop) && fs::is_regular_file(newTop)) {
                                                std::ifstream in(newTop);
                                                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                                                std::ofstream out(patchDir / "last.patch");
                                                out << content;
                                            }
                                        }
                                    } else {
                                        fs::remove(patchDir / "last.patch", ec);
                                        fs::remove(patchDir / "last_patch.json", ec);
                                    }
                                } catch (...) {}

                                std::cout << GREEN << "✔ Successfully reverted last patch via Git." << RESET << std::endl;
                            } else {
                                std::cout << RED << "✖ Patch undo failed (git apply -R). Falling back to file undo..." << RESET << std::endl;
                            }
                        } else {
                            std::cout << GRAY << "Undo cancelled." << RESET << std::endl;
                        }
                        break;
                    }

                    // 无论是否成功，都继续走后续 file undo（若补丁撤销成功，此处一般无改动）
                }
            } catch (...) {}

            std::string lastFile = mcpManager.getLastModifiedFile("builtin");
            if (lastFile.empty()) {
                std::cout << YELLOW << "⚠ No recent file modifications recorded." << RESET << std::endl;
            } else {
                // 1. 根据全局状态决定回退策略
                bool useGit = false;
                if (g_hasGit) {
#ifdef _WIN32
                    std::string gitCheckCmd = "git ls-files --error-unmatch \"" + lastFile + "\" 2>nul";
#else
                    std::string gitCheckCmd = "git ls-files --error-unmatch \"" + lastFile + "\" 2>/dev/null";
#endif
                    useGit = (system(gitCheckCmd.c_str()) == 0);
                }

                // Backup path mapping must handle absolute paths (which would otherwise escape backupDir)
                fs::path absolutePath = fs::absolute(fs::u8path(path));
                fs::path backupDir = absolutePath / ".photon" / "backups";
                fs::path lf = fs::u8path(lastFile);
                fs::path backupRel;
                if (!lf.is_absolute()) {
                    backupRel = lf;
                } else {
                    fs::path rn = lf.root_name();
                    fs::path rp = lf.relative_path();
                    if (!rn.empty()) {
                        std::string s = rn.u8string();
                        for (auto& ch : s) if (ch == ':' || ch == '\\') ch = '_';
                        backupRel = fs::path("abs") / fs::u8path(s) / rp;
                    } else {
                        backupRel = fs::path("abs") / rp;
                    }
                }
                fs::path backupPath = backupDir / backupRel;
                bool hasBackup = fs::exists(backupPath);

                if (!useGit && !hasBackup) {
                    std::cout << RED << "✖ No Git history or backup found for: " << lastFile << RESET << std::endl;
                    continue;
                }

                std::cout << YELLOW << BOLD << "Reverting changes in: " << RESET << lastFile << std::endl;

                // 预览逻辑
                if (useGit) {
#ifdef _WIN32
                    system(("git diff --color=always \"" + lastFile + "\"").c_str());
#else
                    system(("git diff --color=always \"" + lastFile + "\"").c_str());
#endif
                } else {
                    std::ifstream bFile(backupPath);
                    std::string bContent((std::istreambuf_iterator<char>(bFile)), std::istreambuf_iterator<char>());
                    showGitDiff(lastFile, bContent);
                }

                while (true) {
                    std::cout << "\n " << YELLOW << BOLD << "⚠  UNDO CONFIRMATION" << RESET << std::endl;
                    std::cout << GRAY << "   Revert changes in: " << RESET << lastFile 
                              << (useGit ? BLUE + " (via Git)" : GREEN + " (via Backup)") << RESET << std::endl;
                    std::cout << "   " << BOLD << "[y]" << RESET << " Yes  " 
                              << BOLD << "[n]" << RESET << " No  " 
                              << BOLD << "[v]" << RESET << " View Diff" << std::endl;
                    std::cout << " " << CYAN << BOLD << "❯ " << RESET << std::flush;

                    std::string confirm;
                    std::getline(std::cin, confirm);
                    std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
                    
                    if (confirm == "v") {
                        if (useGit) {
#ifdef _WIN32
                            system(("git diff --color=always \"" + lastFile + "\"").c_str());
#else
                            system(("git diff --color=always \"" + lastFile + "\"").c_str());
#endif
                        } else {
                            std::ifstream bFile(backupPath);
                            std::string bContent((std::istreambuf_iterator<char>(bFile)), std::istreambuf_iterator<char>());
                            showGitDiff(lastFile, bContent, true);
                        }
                        continue;
                    }

                    if (confirm == "y" || confirm == "yes") {
                        bool success = false;
                        if (useGit) {
#ifdef _WIN32
                            success = (system(("git checkout -- \"" + lastFile + "\"").c_str()) == 0);
#else
                            success = (system(("git restore \"" + lastFile + "\"").c_str()) == 0);
#endif
                            if (success) std::cout << GREEN << "✔ Successfully restored via Git." << RESET << std::endl;
                        } 
                        
                        if (!success && hasBackup) {
                            nlohmann::json result = mcpManager.callTool("builtin", "file_undo", {{"path", lastFile}});
                            if (result.contains("content")) {
                                std::cout << GREEN << "✔ " << result["content"][0]["text"].get<std::string>() << " (via Backup)" << RESET << std::endl;
                                success = true;
                            }
                        }

                        if (success) {
                            messages.push_back({{"role", "user"}, {"content", "[SYSTEM]: User has undone your last change to " + lastFile + ". Please reflect on why the change was reverted."}});
                        } else {
                            std::cout << RED << "✖ Undo failed: No available recovery method." << RESET << std::endl;
                        }
                    } else {
                        std::cout << GRAY << "Undo cancelled." << RESET << std::endl;
                    }
                    break;
                }
            }
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", userInput}});

        bool continues = true;
        int iteration = 0;
        int max_iterations = 50; // Increased default limit to 50 rounds
        bool authorizeAll = false; // Batch authorization flag for this turn

        while (continues && iteration < max_iterations) {
            iteration++;
            // Inject read summaries before context management
            injectReadSummaries();
            // Apply context management
            messages = contextManager.manage(messages);

        // Update status bar during thinking
        // UIManager::getInstance().updateStatus(cfg.llm.model, (int)contextManager.getSize(messages), mcpManager.getTotalTaskCount());

        nlohmann::json response = llmClient->chatWithTools(messages, llmTools);
            
            if (response.is_null() || !response.contains("choices") || response["choices"].empty()) {
                break;
            }

            auto& choice = response["choices"][0];
            auto& message = choice["message"];
            
            // Normalize assistant message before appending: Kimi may reject if content is array in request
            nlohmann::json msgToAppend = message;
            if (msgToAppend.contains("content") && msgToAppend["content"].is_array()) {
                std::string flat;
                for (auto& part : msgToAppend["content"]) {
                    if (part.is_object() && part.contains("text") && part["text"].is_string())
                        flat += part["text"].get<std::string>();
                }
                msgToAppend["content"] = flat.empty() ? "" : flat;
            }
            messages.push_back(msgToAppend);

            // 1. Handle content (Thought or final response)
            if (message.contains("content") && !message["content"].is_null()) {
                std::string content;
                if (message["content"].is_string())
                    content = message["content"].get<std::string>();
                else if (message["content"].is_array()) {
                    for (auto& part : message["content"]) {
                        if (part.is_object() && part.contains("text") && part["text"].is_string())
                            content += part["text"].get<std::string>();
                    }
                }
                if (!content.empty()) {
                    if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                        Logger::getInstance().thought(renderMarkdown(content));
                    } else {
                        // Final response
                        std::cout << "\n" << MAGENTA << BOLD << "🐼 Photon " << RESET << GRAY << "❯ " << RESET << renderMarkdown(content) << std::endl;
                    }
                }
            }

            // 2. Handle Tool Calls
            if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                continues = true;
                for (auto& toolCall : message["tool_calls"]) {
                    // 安全检查: 确保 toolCall 是对象且包含 function
                    if (!toolCall.is_object() || !toolCall.contains("function")) {
                        Logger::getInstance().error("Invalid tool_call format: " + toolCall.dump());
                        continue;
                    }
                    
                    auto& func = toolCall["function"];
                    if (!func.is_object() || !func.contains("name") || !func.contains("arguments")) {
                        Logger::getInstance().error("Invalid function format in tool_call: " + func.dump());
                        continue;
                    }
                    
                    std::string fullName = func["name"].get<std::string>();
                    std::string argsStr = func["arguments"].is_string() ? 
                        func["arguments"].get<std::string>() : func["arguments"].dump();
                    nlohmann::json args;
                    try {
                        args = nlohmann::json::parse(argsStr);
                    } catch (...) {
                        args = nlohmann::json::object();
                        Logger::getInstance().warn("Tool args parse failed for " + fullName + ": " + argsStr);
                    }

                    // Policy Enforcement: Discourage full-file overwrites in tool calls
                    if (fullName == "builtin__write" || fullName == "write") {
                        if (args.contains("path") && args.contains("content") && 
                            !args.contains("operation") && !args.contains("search") && 
                            fs::exists(fs::absolute(fs::u8path(path)) / fs::u8path(args["path"].get<std::string>()))) {
                            
                            // Check if it's a very small file or a new file (not exists is already checked)
                            // We'll allow it but add a warning to the next message
                            // Actually, let's just log it for now to see how often it happens
                            // Logger::getInstance().warning("Full-file overwrite detected for: " + args["path"].get<std::string>());
                        }
                    }

                    // Split server_name__tool_name
                    size_t pos = fullName.find("__");
                    std::string serverName;
                    std::string toolName;
                    
                    if (pos != std::string::npos) {
                        serverName = fullName.substr(0, pos);
                        toolName = fullName.substr(pos + 2);
                    } else {
                        // 新的 CoreTools 没有 server__ 前缀
                        serverName = "core";
                        toolName = fullName;
                    }

                        if (toolName == "bash_execute" && args.contains("command")) {
                            std::string cmd = args["command"];
                            if (isBashReadCommand(cmd)) {
                                nlohmann::json loopError = {
                                    {"error", "Bash read commands are disabled. Use read/search/plan instead."}
                                };
                                messages.push_back({
                                    {"role", "tool"},
                                    {"tool_call_id", toolCall["id"]},
                                    {"name", fullName},
                                    {"content", loopError.dump()}
                                });
                                continue;
                            }
                        }

                        // 优化：检测重复搜索死循环
                        if (toolName.find("search") != std::string::npos) {
                            std::string query = args.value("query", "");
                            if (!query.empty()) {
                                if (recentQueries.count(query)) {
                                    std::cout << YELLOW << "  ⚠ Detected repetitive search loop. Forcing strategy shift." << RESET << std::endl;
                                    nlohmann::json loopError = {{"error", "Repetitive search detected. Please change your search strategy or use web_fetch to read existing results."}};
                                    messages.push_back({{"role", "tool"}, {"tool_call_id", toolCall["id"]}, {"name", fullName}, {"content", loopError.dump()}});
                                    continue;
                                }
                                recentQueries.insert(query);
                            }
                        }

                        Logger::getInstance().action(serverName + "::" + toolName + " " + args.dump());
                        
                        // Human-in-the-loop: Confirmation for risky tools
                        std::string confirm = "y"; // Default to yes for non-risky tools
                        if (isRiskyTool(toolName) && !authorizeAll) {
                            // 1. Pre-confirmation Preview (Optional, but helpful)
                            // We don't automatically show diff here to avoid clutter, 
                            // user can request it with 'v'.

                            while (true) {
                                std::cout << "\n " << YELLOW << BOLD << "⚠  CONFIRMATION REQUIRED" << RESET << std::endl;
                                std::cout << GRAY << "   Tool: " << RESET << serverName << "::" << toolName << std::endl;
                                std::cout << "   " << BOLD << "[y]" << RESET << " Yes  " 
                                          << BOLD << "[n]" << RESET << " No  " 
                                          << BOLD << "[a]" << RESET << " All  " 
                                          << BOLD << "[v]" << RESET << " View Diff" << std::endl;
                                std::cout << " " << CYAN << BOLD << "❯ " << RESET << std::flush;
                                
                                std::string input;
                                std::getline(std::cin, input);
                                std::transform(input.begin(), input.end(), input.begin(), ::tolower);
                                
                                if (input == "v") {
                                    // Specialized Diff Preview for file operations
                                    if ((toolName == "file_write" || toolName == "write") && args.contains("content") && args.contains("path") && !args.contains("operation") && !args.contains("search")) {
                                        showGitDiff(args["path"], args["content"], true);
                                    } else if ((toolName == "file_edit_lines" || toolName == "write") && args.contains("path") && args.contains("operation")) {
                                        // Simulate line-based edit
                                        std::string path = args["path"];
                                        std::ifstream inFile(path);
                                        if (inFile.is_open()) {
                                            std::vector<std::string> lines;
                                            std::string line;
                                            while (std::getline(inFile, line)) lines.push_back(line);
                                            inFile.close();
                                            std::string op = args["operation"];
                                            int start = args["start_line"];
                                            int end = args.value("end_line", start);
                                            std::string content = args.value("content", "");
                                            std::vector<std::string> newLines;
                                            std::istringstream iss(content);
                                            std::string nl;
                                            while (std::getline(iss, nl)) newLines.push_back(nl);
                                            if (op == "replace" && start >= 1 && start <= (int)lines.size()) {
                                                if (end > (int)lines.size()) end = (int)lines.size();
                                                lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                                lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                            } else if (op == "insert") {
                                                if (start < 1) start = 1;
                                                if (start > (int)lines.size() + 1) start = (int)lines.size() + 1;
                                                lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                            } else if (op == "delete" && start >= 1 && start <= (int)lines.size()) {
                                                if (end > (int)lines.size()) end = (int)lines.size();
                                                lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                            }
                                            std::string simulated;
                                            for (size_t i = 0; i < lines.size(); ++i) simulated += lines[i] + (i == lines.size() - 1 ? "" : "\n");
                                            showGitDiff(path, simulated, true);
                                        }
                                    } else if ((toolName == "edit_batch_lines" || toolName == "write") && args.contains("edits")) {
                                        std::cout << YELLOW << BOLD << "\n📦 BATCH EDIT PREVIEW:" << RESET << std::endl;
                                        for (const auto& edit : args["edits"]) {
                                            std::string path = edit["path"];
                                            std::ifstream inFile(path);
                                            if (inFile.is_open()) {
                                                std::vector<std::string> lines;
                                                std::string line;
                                                while (std::getline(inFile, line)) lines.push_back(line);
                                                inFile.close();
                                                std::string op = edit["operation"];
                                                int start = edit["start_line"];
                                                int end = edit.value("end_line", start);
                                                std::string content = edit.value("content", "");
                                                std::vector<std::string> newLines;
                                                std::istringstream iss(content);
                                                std::string nl;
                                                while (std::getline(iss, nl)) newLines.push_back(nl);
                                                if (op == "replace" && start >= 1 && start <= (int)lines.size()) {
                                                    if (end > (int)lines.size()) end = (int)lines.size();
                                                    lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                                    lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                                } else if (op == "insert") {
                                                    if (start < 1) start = 1;
                                                    if (start > (int)lines.size() + 1) start = (int)lines.size() + 1;
                                                    lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                                } else if (op == "delete" && start >= 1 && start <= (int)lines.size()) {
                                                    if (end > (int)lines.size()) end = (int)lines.size();
                                                    lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                                }
                                                std::string simulated;
                                                for (size_t i = 0; i < lines.size(); ++i) simulated += lines[i] + (i == lines.size() - 1 ? "" : "\n");
                                                showGitDiff(path, simulated, true);
                                            }
                                        }
                                    } else if (toolName == "write" && args.contains("search") && args.contains("replace") && args.contains("path")) {
                                        std::string path = args["path"];
                                        std::ifstream inFile(path);
                                        if (inFile.is_open()) {
                                            std::string fullContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
                                            inFile.close();
                                            std::string searchStr = args["search"];
                                            std::string replaceStr = args["replace"];
                                            size_t pos = fullContent.find(searchStr);
                                            if (pos != std::string::npos) {
                                                fullContent.replace(pos, searchStr.length(), replaceStr);
                                                showGitDiff(path, fullContent, true);
                                            }

                                        }
                                    } else {
                                        std::cout << GRAY << "   (No preview available for this tool)" << RESET << std::endl;
                                    }
                                    continue; // Ask again
                                }

                                if (input == "a" || input == "all") {
                                    authorizeAll = true;
                                    mcpManager.setAllAuthorized(true);
                                    confirm = "y";
                                    break;
                                }

                                if (input != "y" && input != "yes") {
                                    std::cout << RED << "✖  Action cancelled by user." << RESET << std::endl;
                                    messages.push_back({
                                        {"role", "tool"},
                                        {"tool_call_id", toolCall["id"]},
                                        {"name", fullName},
                                        {"content", "{\"error\": \"Action cancelled by user.\"}"}
                                    });
                                    continues = false; // Stop the loop for this turn
                                    confirm = "n";
                                    break;
                                }
                                confirm = "y";
                                break; // confirmed 'y'
                            }
                            if (!continues) break; // Break out of tool call loop if cancelled
                        }

                        if (confirm == "y" || confirm == "yes") {
                            nlohmann::json result;
                            
                            // ============================================================
                            // Constitution v2.0: Hard Constraint Validation
                            // ============================================================
                            auto validation = ConstitutionValidator::validateToolCall(toolName, args);
                            if (!validation.valid) {
                                std::cout << RED << "✖ Constitution Violation" << RESET << std::endl;
                                std::cout << GRAY << "  Constraint: " << validation.constraint << RESET << std::endl;
                                std::cout << GRAY << "  Error: " << validation.error << RESET << std::endl;
                                
                                result["error"] = "Constitution violation: " + validation.error;
                                Logger::getInstance().error("Constitution violation in " + toolName + ": " + validation.error);
                                
                                // Add error response to messages
                                std::string errorMsg = "Constitution violation (" + validation.constraint + "): " + validation.error;
                                messages.push_back({
                                    {"role", "tool"},
                                    {"tool_call_id", toolCall["id"]},
                                    {"content", errorMsg}
                                });
                                continue; // Skip execution, move to next tool call
                            }
                            
                            // ============================================================
                            // 新架构: 优先使用 ToolRegistry 中的核心工具
                            // ============================================================
                            if (toolRegistry.hasTool(toolName)) {
                                // 使用新的 ToolRegistry
                                std::cout << GRAY << "  [Using CoreTools::" << toolName << "]" << RESET << std::endl;
                                result = toolRegistry.executeTool(toolName, args);
                            } else {
                                // 回退到旧的 MCP 系统 (外部工具 / 遗留工具)
                                bool tempAuth = (isRiskyTool(toolName) && !authorizeAll);
                                if (tempAuth) mcpManager.setAllAuthorized(true);
                                
                                result = mcpManager.callTool(serverName, toolName, args);
                                
                                if (tempAuth) mcpManager.setAllAuthorized(false);
                            }

                            if (result.contains("error")) {
                                Logger::getInstance().error("Tool " + toolName + " failed: " + result["error"].get<std::string>());
                                try {
                                    Logger::getInstance().error("Tool " + toolName + " args: " + args.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
                                    Logger::getInstance().error("Tool " + toolName + " result: " + result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
                                } catch (...) {
                                    Logger::getInstance().error("Tool " + toolName + " - failed to serialize args/result");
                                }
                            } else if (result.is_null() || (result.is_object() && result.empty())) {
                                Logger::getInstance().warn("Tool " + toolName + " returned empty result.");
                                try {
                                    Logger::getInstance().warn("Tool " + toolName + " args: " + args.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace));
                                } catch (...) {
                                    Logger::getInstance().warn("Tool " + toolName + " - failed to serialize args");
                                }
                            }
                            
                            if ((toolName == "read" || toolName.rfind("read_", 0) == 0) && !result.contains("error")) {
                                std::string readText = extractReadText(result);
                                if (!readText.empty() && !isReadRejection(readText)) {
                                    const size_t kMaxSummaryInput = 6000;
                                    if (readText.size() > kMaxSummaryInput) {
                                        readText = readText.substr(0, kMaxSummaryInput);
                                    }
                                    std::string key = buildReadKey(args);
                                    std::string summaryPrompt =
                                        "请对以下 read 结果做 1-3 条要点摘要，保留文件路径/范围或标签：\n" + readText;
                                    std::string summary = llmClient->summarize(summaryPrompt);
                                    storeReadSummary(key, summary);
                                }
                            }

                            // Add tool result to messages
                            // Kimi 要求 content 为 []object，即 [{"type":"text","text":"..."}]
                            std::string textContent;
                            try {
                                if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
                                    const auto& first = result["content"][0];
                                    if (first.contains("text") && first["text"].is_string()) {
                                        textContent = first["text"].get<std::string>();
                                    } else {
                                        textContent = first.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
                                    }
                                } else if (result.contains("error")) {
                                    textContent = result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
                                } else {
                                    textContent = result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
                                }
                            } catch (const std::exception& e) {
                                textContent = "[Tool result serialization failed: invalid UTF-8]";
                            }
                            
                            if (cfg.agent.enableDebug) {
                                size_t preview = std::min(textContent.size(), size_t(800));
                                std::cout << "[Debug] Tool result to model (length=" << textContent.size() << ", preview " << preview << " chars):\n---\n"
                                          << textContent.substr(0, preview) << (textContent.size() > preview ? "\n..." : "") << "\n---\n" << std::endl;
                            }
                            
                            nlohmann::json contentArray = nlohmann::json::array({
                                nlohmann::json::object({{"type", "text"}, {"text", textContent}})
                            });
                            
                            messages.push_back({
                                {"role", "tool"},
                                {"tool_call_id", toolCall["id"]},
                                {"content", contentArray}
                            });
                        }
                }
                continues = true;
            } else {
                // No more tool calls, we are done
                continues = false;
            }

            if (iteration >= max_iterations && continues) {
                std::cout << "\n " << YELLOW << BOLD << "⚠  LIMIT REACHED" << RESET << std::endl;
                std::cout << GRAY << "   Maximum thinking steps (" << max_iterations << ") reached." << RESET << std::endl;
                std::cout << "   " << BOLD << "[y]" << RESET << " Continue (20 steps)  " 
                          << BOLD << "[n]" << RESET << " Stop" << std::endl;
                std::cout << " " << CYAN << BOLD << "❯ " << RESET << std::flush;

                std::string confirm;
                std::getline(std::cin, confirm);
                std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
                if (confirm == "y" || confirm == "yes") {
                    max_iterations += 20;
                } else {
                    std::cout << YELLOW << "Stopping loop as requested." << RESET << std::endl;
                    continues = false;
                }
            }
        }

        // Final Status Display
        size_t currentSize = contextManager.getSize(messages);
        int taskCount = mcpManager.getTotalTaskCount();
        std::cout << GRAY << "─── " 
                  << CYAN << "Model: " << BOLD << cfg.llm.model << RESET << GRAY << " | "
                  << CYAN << "Context: " << BOLD << currentSize << RESET << GRAY << " chars | "
                  << CYAN << "Tasks: " << BOLD << taskCount << RESET << GRAY << " active ───" 
                  << RESET << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    } catch (const std::exception& e) {
        Logger::getInstance().error("FATAL ERROR: " + std::string(e.what()));
        std::cerr << "\n" << RED << BOLD << "█ FATAL ERROR: " << RESET << e.what() << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        Logger::getInstance().error("UNKNOWN FATAL ERROR");
        std::cerr << "\n" << RED << BOLD << "█ UNKNOWN FATAL ERROR" << RESET << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
