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
#include <cstdlib>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
    #include <winsock2.h>
#endif
#include "utils/FileManager.h"
#include "core/LLMClient.h"
#include "core/ContextManager.h"
#include "core/ConfigManager.h"
#include "mcp/MCPClient.h"
#include "mcp/MCPManager.h"
#include "mcp/LSPClient.h"
#include "utils/SkillManager.h"
#include "utils/SymbolManager.h"
#include "utils/Logger.h"
#include "utils/RegexSymbolProvider.h"
#include "utils/TreeSitterSymbolProvider.h"
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
        "write", "file_write", "file_create", "file_edit_lines", "edit_batch_lines", "diff_apply", 
        "bash_execute", "git_operations", "python_sandbox", "pip_install", "schedule"
    };
    return risky.count(toolName) > 0;
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
        emptyFile = path + ".photon.empty";
        std::ofstream out(emptyFile);
        out.close();
        originalPath = emptyFile;
    }

    std::string tmpPath = path + ".photon.tmp";
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
    g_hasGit = (system("git rev-parse --is-inside-work-tree >/dev/null 2>&1") == 0);
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
        
        // Check if config.json exists in current directory, if not try other locations
        if (argc < 3) {
            if (fs::exists(fs::u8path("config.json"))) {
                configPath = "config.json";
            } else if (fs::exists(fs::u8path("../config.json"))) {
                configPath = "../config.json";
            } else if (!exeDir.empty() && fs::exists(exeDir / "config.json")) {
                configPath = (exeDir / "config.json").u8string();
            }
        } else {
            configPath = argv[2];
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
            globalDataPath = fs::u8path(path) / ".photon";
        }
        skillManager.syncAndLoad(cfg.agent.skillRoots, globalDataPath.u8string());
    });
    
    // Initialize Symbol Manager and start async scan
    SymbolManager symbolManager(path);
    symbolManager.setFallbackOnEmpty(cfg.agent.symbolFallbackOnEmpty);
    
    // Initialize Semantic Manager
    auto semanticManager = std::make_shared<SemanticManager>(path, llmClient);
    
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
    semanticManager->startAsyncIndexing();

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

        LspInitResult result;
        for (auto& f : lspFutures) {
            auto item = f.get();
            if (item.ok) {
                result.availableServers.push_back(item.server);
                result.clients.push_back(std::move(item.client));
            }
        }
        return result;
    });

    // Inject into Builtin Tools
    if (auto* builtin = dynamic_cast<InternalMCPClient*>(mcpManager.getClient("builtin"))) {
        builtin->setSkillManager(&skillManager);
        builtin->setSymbolManager(&symbolManager);
        builtin->setSemanticManager(semanticManager.get());
    }

    std::cout << GREEN << "  ✔ Engine active. " << RESET << std::endl;

    // Get tools and format
    auto mcpTools = mcpManager.getAllTools();
    nlohmann::json llmTools = formatToolsForLLM(mcpTools);

    std::cout << "  " << CYAN << "Model  " << RESET << " : " << PURPLE << cfg.llm.model << RESET << std::endl;
    
    std::cout << "\n  " << YELLOW << "Shortcuts:" << RESET << std::endl;
    std::cout << GRAY << "  ┌──────────────────────────────────────────────────────────┐" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "tools   " << RESET << GRAY << " List all sensors   " 
              << "│ " << RESET << BOLD << "undo    " << RESET << GRAY << " Revert change     " << "│" << RESET << std::endl;
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

    std::string systemPrompt = "You are Photon, an autonomous AI agentic intelligence. Your mission is to assist with complex engineering tasks through sensing (tools), reasoning, and acting.\n" +
                               cfg.llm.systemRole + "\n" +
                               "PhotonRule v1.0:\n" +
                               "1. MIN_IO: No full-file reads >500 lines.\n" +
                               "2. PATCH_ONLY: No full-file overwrites. Use surgical edits.\n" +
                               "3. SEARCH_FIRST: Use 'context_plan' to select entry points before reading.\n" +
                               "4. DECOUPLE: Split files >1000 lines.\n" +
                               "5. JSON_STRICT: Validate schemas.\n" +
                               "6. ASYNC_SAFE: Respect async flows.\n\n" +
                               skillManager.getSystemPromptAddition() + "\n" +
                               "Current working directory for your tools: " + path + "\n" +
                               "Current system time: " + date_ss.str() + "\n" +
                               "CRITICAL GUIDELINES:\n" +
                               "1. CHAIN-OF-THOUGHT: Reason step-by-step. Justify every action before execution.\n" +
                               "2. CONTEXT-FIRST: Use 'context_plan' to generate a retrieval plan (entry -> calls -> dependencies), then read only within the plan. Use 'lsp_definition', 'lsp_references', and 'generate_logic_map' to navigate. DO NOT read entire files just to find a symbol.\n" +
                               "3. SURGICAL EDITS (MANDATORY): You MUST use targeted line-based edits ('operation': 'insert'/'replace'/'delete') or 'search'/'replace' blocks in the 'write' tool. Full-file overwrites (providing ONLY 'path' and 'content') are STRICTLY FORBIDDEN for existing files unless the change affects >80% of the content. This is to prevent accidental code loss and minimize token usage.\n" +
                               "4. PRESERVE CONTEXT: When using 'replace' or 'delete', ensure the line numbers are accurate by reading the file immediately before editing.\n" +
                               "5. TOKEN EFFICIENCY: Use line-range reads (start_line/end_line) in the 'read' tool to examine only relevant code. Full-file reads are a last resort.\n" +
                               "6. ADAPTIVE STRATEGY: Detect repetitive loops or tool failures early. Pivot to alternative approaches immediately.\n" +
                               "7. STRICT VERIFICATION: Always validate changes through 'diagnostics_check' and targeted 'read' calls to ensure zero regressions.\n" +
                               "8. PROACTIVE CLARIFICATION: If a task is ambiguous or high-risk, pause and ask the user for guidance.";
    
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
        return text.find("⚠️  请先使用 context_plan") != std::string::npos ||
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
            FILE* pipe = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
            if (pipe) {
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    branch = buffer;
                    if (!branch.empty() && branch.back() == '\n') branch.pop_back();
                }
                pclose(pipe);
            }
            
            bool isDirty = (system("git diff --quiet 2>/dev/null") != 0);
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

            // Inject into Builtin Tools
            if (auto* builtin = dynamic_cast<InternalMCPClient*>(mcpManager.getClient("builtin"))) {
                builtin->setSkillManager(&skillManager);
                builtin->setSymbolManager(&symbolManager);
                builtin->setSemanticManager(semanticManager.get());
                if (!lspClients.empty()) builtin->setLSPClients(lspByExt, lspFallback);
            }

            symbolManager.setLSPClients(lspByExt, lspFallback);
            symbolManager.startAsyncScan();
            symbolManager.startWatching(5);
            
            std::cout << "                                        \r" << std::flush;
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
            for (const auto& t : mcpTools) {
                std::string server = t.value("server_name", "unknown");
                std::string name = t.value("name", "unknown");
                std::string desc = t.value("description", "No description");
                
                std::cout << (server == "builtin" ? GREEN + "[Built-in] " : BLUE + "[External] ") 
                          << BOLD << server << "::" << name << RESET << std::endl;
                std::cout << "  " << desc << std::endl;
            }
            std::cout << CYAN << "-----------------------\n" << RESET << std::endl;
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
            nlohmann::json res = mcpManager.callTool("builtin", "memory_list", {});
            if (res.contains("content") && res["content"][0].contains("text")) {
                std::cout << renderMarkdown(res["content"][0]["text"].get<std::string>()) << std::endl;
            } else {
                std::cout << "Failed to retrieve memory list." << std::endl;
            }
            std::cout << CYAN << "------------------------\n" << RESET << std::endl;
            continue;
        }

        if (userInput == "undo") {
            std::string lastFile = mcpManager.getLastModifiedFile("builtin");
            if (lastFile.empty()) {
                std::cout << YELLOW << "⚠ No recent file modifications recorded." << RESET << std::endl;
            } else {
                // 1. 根据全局状态决定回退策略
                bool useGit = false;
                if (g_hasGit) {
                    std::string gitCheckCmd = "git ls-files --error-unmatch \"" + lastFile + "\" 2>/dev/null";
                    useGit = (system(gitCheckCmd.c_str()) == 0);
                }

                fs::path backupPath = fs::u8path(path) / ".photon" / "backups" / fs::u8path(lastFile);
                bool hasBackup = fs::exists(backupPath);

                if (!useGit && !hasBackup) {
                    std::cout << RED << "✖ No Git history or backup found for: " << lastFile << RESET << std::endl;
                    continue;
                }

                std::cout << YELLOW << BOLD << "Reverting changes in: " << RESET << lastFile << std::endl;

                // 预览逻辑
                if (useGit) {
                    system(("git diff --color=always \"" + lastFile + "\"").c_str());
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
                            system(("git diff --color=always \"" + lastFile + "\"").c_str());
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
                            success = (system(("git restore \"" + lastFile + "\"").c_str()) == 0);
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
            
            // Add assistant's message to the conversation history
            messages.push_back(message);

            // 1. Handle content (Thought or final response)
            if (message.contains("content") && !message["content"].is_null()) {
                std::string content = message["content"].get<std::string>();
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
                    std::string fullName = toolCall["function"]["name"];
                    std::string argsStr = toolCall["function"]["arguments"];
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
                            fs::exists(fs::u8path(path) / fs::u8path(args["path"].get<std::string>()))) {
                            
                            // Check if it's a very small file or a new file (not exists is already checked)
                            // We'll allow it but add a warning to the next message
                            // Actually, let's just log it for now to see how often it happens
                            // Logger::getInstance().warning("Full-file overwrite detected for: " + args["path"].get<std::string>());
                        }
                    }

                    // Split server_name__tool_name
                    size_t pos = fullName.find("__");
                    if (pos != std::string::npos) {
                        std::string serverName = fullName.substr(0, pos);
                        std::string toolName = fullName.substr(pos + 2);

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
                            // If user confirmed this specific call, temporarily authorize it in the MCP client
                            bool tempAuth = (isRiskyTool(toolName) && !authorizeAll);
                            if (tempAuth) mcpManager.setAllAuthorized(true);
                            
                            nlohmann::json result = mcpManager.callTool(serverName, toolName, args);
                            
                            // Revert authorization if it was temporary
                            if (tempAuth) mcpManager.setAllAuthorized(false);

                            if (result.contains("error")) {
                                Logger::getInstance().error("Tool " + toolName + " failed: " + result["error"].get<std::string>());
                                Logger::getInstance().error("Tool " + toolName + " args: " + args.dump());
                                Logger::getInstance().error("Tool " + toolName + " result: " + result.dump());
                            } else if (result.is_null() || (result.is_object() && result.empty())) {
                                Logger::getInstance().warn("Tool " + toolName + " returned empty result.");
                                Logger::getInstance().warn("Tool " + toolName + " args: " + args.dump());
                            }
                            
                            if (toolName == "read" && !result.contains("error")) {
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
                            messages.push_back({
                                {"role", "tool"},
                                {"tool_call_id", toolCall["id"]},
                                {"name", fullName},
                                {"content", result.dump()}
                            });
                        }
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
