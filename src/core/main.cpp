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
#include "utils/RegexSymbolProvider.h"
#include "utils/TreeSitterSymbolProvider.h"
#ifdef PHOTON_ENABLE_TREESITTER
#include "tree-sitter-cpp.h"
#include "tree_sitter/tree-sitter-python.h"
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
                fs::path exe = candidate;
                exe += ".exe";
                if (fs::exists(exe) && fs::is_regular_file(exe)) {
                    return exe.u8string();
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
    return {};
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
        addServer("clangd", "clangd", {".c", ".cpp", ".h", ".hpp"});
    }

    if (!findExecutableInPath({"pyright-langserver"}).empty()) {
        addServer("pyright", "pyright-langserver --stdio", {".py"});
    } else if (!findExecutableInPath({"pylsp"}).empty()) {
        addServer("pylsp", "pylsp", {".py"});
    }

    return detected;
}

bool isRiskyTool(const std::string& toolName) {
    static const std::unordered_set<std::string> risky = {
        "file_write", "file_edit_lines", "diff_apply", "bash_execute", "git_operations"
    };
    return risky.count(toolName) > 0;
}

void printStatusBar(const std::string& model, size_t contextSize, int taskCount) {
    std::cout << GRAY << "─── " 
              << CYAN << "Model: " << BOLD << model << RESET << GRAY << " | "
              << CYAN << "Context: " << BOLD << contextSize << RESET << GRAY << " chars | "
              << CYAN << "Tasks: " << BOLD << taskCount << RESET << GRAY << " active ───" 
              << RESET << std::endl;
}

void showGitDiff(const std::string& path, const std::string& newContent, bool forceFull = false) {
    std::string tmpPath = path + ".photon.tmp";
    std::ofstream tmpFile(tmpPath);
    if (!tmpFile.is_open()) return;
    tmpFile << newContent;
    tmpFile.close();

    std::string cmd = "git diff --no-index --color=always \"" + path + "\" \"" + tmpPath + "\" 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        std::vector<std::string> diffLines;
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            if (line.find("--- a/") == 0 || line.find("+++ b/") == 0 || line.find("diff --git") == 0) continue;
            diffLines.push_back(line);
        }
        pclose(pipe);

        std::cout << GRAY << "┌─── " << BOLD << "GIT DIFF PREVIEW" << RESET << GRAY << " (Target: " << path << ") ────────────────" << RESET << std::endl;
        
        const size_t THRESHOLD = 20;
        if (!forceFull && diffLines.size() > THRESHOLD) {
            // Show first 10 lines
            for (size_t i = 0; i < 10 && i < diffLines.size(); ++i) std::cout << diffLines[i];
            
            // Show folding message
            std::cout << YELLOW << BOLD << "  ... (" << (diffLines.size() - 15) << " lines folded) ..." << RESET << std::endl;
            std::cout << GRAY << "  Tip: Enter 'v' at the prompt to see full diff." << RESET << std::endl;
            
            // Show last 5 lines
            size_t startLast = diffLines.size() > 5 ? diffLines.size() - 5 : 0;
            if (startLast < 10) startLast = 10; // Avoid overlapping
            for (size_t i = startLast; i < diffLines.size(); ++i) std::cout << diffLines[i];
        } else {
            for (const auto& line : diffLines) std::cout << line;
            if (diffLines.empty()) std::cout << GRAY << "  (No changes detected or file is new)" << RESET << std::endl;
        }
        
        std::cout << GRAY << "└──────────────────────────────────────────────────────────────────" << RESET << std::endl;
    }
    std::remove(tmpPath.c_str());
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
    for (const auto& line : lines) std::cout << line << std::endl;
    std::cout << frame << std::endl;
    std::cout << GRAY << "        ─── " << ITALIC << "The Native Agentic Core v1.0 [Cyber Falcon Edition]" << RESET << GRAY << " ───\n" << std::endl;
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

    // Global exception handler to prevent silent crash
    try {
        printLogo(); // 先展示 Logo 动画

        std::string path = ".";
        
        // Determine executable directory for default config search
        fs::path exeDir;
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
    if (cfg.agent.useBuiltinTools) {
        std::cout << GRAY << "  [1/2] Initializing built-in core tools..." << RESET << "\r" << std::flush;
        mcpManager.initBuiltin(path, cfg.agent.searchApiKey);
    }
    
    std::cout << GRAY << "  [2/2] Connecting to external MCP servers..." << RESET << "\r" << std::flush;
    int externalCount = mcpManager.initFromConfig(cfg.mcpServers);

    // Initialize Skill Manager
    SkillManager skillManager;

    // 1. Load bundled internal skills (Low priority)
    // We already have static skills loaded in constructor, 
    // but we still try to load from directory to allow updating without recompiling.
    try {
        fs::path exeDir;
#ifdef _WIN32
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH) != 0) {
            exeDir = fs::path(szPath).parent_path();
        }
#elif defined(__APPLE__)
        char szPath[4096];
        uint32_t size = sizeof(szPath);
        if (_NSGetExecutablePath(szPath, &size) == 0) {
            exeDir = fs::canonical(szPath).parent_path();
        }
#else
        exeDir = fs::canonical("/proc/self/exe").parent_path();
#endif
        
        fs::path builtinPath;
        if (!exeDir.empty() && fs::exists(exeDir / "builtin_skills")) {
            builtinPath = exeDir / "builtin_skills";
        } else if (fs::exists(fs::path("builtin_skills"))) {
            builtinPath = fs::path("builtin_skills");
        }

        if (!builtinPath.empty() && fs::is_directory(builtinPath)) {
            skillManager.loadFromRoot(builtinPath.u8string(), true); // Mark as builtin
        }
    } catch (...) {}

    // 2. Sync & Load specialized skills from config (High priority, can override)
    // Now syncing to globalDataPath/skills instead of project-local
    std::cout << GRAY << "  [3/3] Syncing & Loading specialized skills..." << RESET << "\r" << std::flush;
    
    // Determine globalDataPath for SkillManager sync
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
    
    // Initialize Symbol Manager and start async scan
    SymbolManager symbolManager(path);
    symbolManager.setFallbackOnEmpty(cfg.agent.symbolFallbackOnEmpty);
#ifdef PHOTON_ENABLE_TREESITTER
    if (cfg.agent.enableTreeSitter) {
        auto treeProvider = std::make_unique<TreeSitterSymbolProvider>();
        treeProvider->registerLanguage("cpp", {".cpp", ".h", ".hpp"}, tree_sitter_cpp());
        treeProvider->registerLanguage("python", {".py"}, tree_sitter_python());
        for (const auto& lang : cfg.agent.treeSitterLanguages) {
            treeProvider->registerLanguageFromLibrary(lang.name, lang.extensions, lang.libraryPath, lang.symbol);
        }
        symbolManager.registerProvider(std::move(treeProvider));
    }
#endif
    symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
    symbolManager.startAsyncScan();
    symbolManager.startWatching(5); // Start real-time file watching (check every 5 seconds)

    // Initialize LSP clients if configured
    std::vector<std::unique_ptr<LSPClient>> lspClients;
    std::unordered_map<std::string, LSPClient*> lspByExt;
    LSPClient* lspFallback = nullptr;
    auto rootUri = cfg.agent.lspRootUri;
    if (rootUri.empty()) {
        rootUri = "file://" + fs::absolute(fs::u8path(path)).u8string();
    }

    if (cfg.agent.lspServers.empty() && !cfg.agent.lspServerPath.empty()) {
        Config::Agent::LSPServer server;
        server.name = "default";
        server.command = cfg.agent.lspServerPath;
        server.extensions = guessExtensionsForCommand(server.command);
        cfg.agent.lspServers.push_back(std::move(server));
    }
    if (cfg.agent.lspServers.empty()) {
        cfg.agent.lspServers = autoDetectLspServers();
    }

    for (const auto& server : cfg.agent.lspServers) {
        if (server.command.empty()) continue;
        auto client = std::make_unique<LSPClient>(server.command, rootUri);
        if (!client->initialize()) continue;
        if (!lspFallback) lspFallback = client.get();
        for (auto ext : server.extensions) {
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            lspByExt[ext] = client.get();
        }
        lspClients.push_back(std::move(client));
    }

    // Inject SkillManager and SymbolManager into Builtin Tools
    if (auto* builtin = dynamic_cast<InternalMCPClient*>(mcpManager.getClient("builtin"))) {
        builtin->setSkillManager(&skillManager);
        builtin->setSymbolManager(&symbolManager);
        if (!lspClients.empty()) {
            builtin->setLSPClients(lspByExt, lspFallback);
        }
    }

    std::cout << GREEN << "  ✔ Engine active. " 
              << BOLD << (cfg.agent.useBuiltinTools ? "Built-in" : "") 
              << (cfg.agent.useBuiltinTools && externalCount > 0 ? " + " : "")
              << (externalCount > 0 ? std::to_string(externalCount) + " External" : "") 
              << (skillManager.getCount() > 0 ? " + " + std::to_string(skillManager.getCount()) + " Skills" : "")
              << " loaded." << RESET << "          " << std::endl;

    // Get all available tools and format them for the LLM
    auto mcpTools = mcpManager.getAllTools();
    nlohmann::json llmTools = formatToolsForLLM(mcpTools);

    std::cout << "  " << CYAN << "Target " << RESET << " : " << BOLD << path << RESET << std::endl;
    std::cout << "  " << CYAN << "Config " << RESET << " : " << BOLD << configPath << RESET << std::endl;
    std::cout << "  " << CYAN << "Model  " << RESET << " : " << PURPLE << cfg.llm.model << RESET << std::endl;
    
    std::cout << "\n  " << YELLOW << "Shortcuts:" << RESET << std::endl;
    std::cout << GRAY << "  ┌──────────────────────────────────────────────────────────┐" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "tools   " << RESET << GRAY << " List all sensors   " 
              << "│ " << RESET << BOLD << "undo    " << RESET << GRAY << " Revert change     " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "skills  " << RESET << GRAY << " List active skills " 
              << "│ " << RESET << BOLD << "compress" << RESET << GRAY << " Summary memory    " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "tasks   " << RESET << GRAY << " List sched tasks   " 
              << "│ " << RESET << BOLD << "clear   " << RESET << GRAY << " Reset context     " << "│" << RESET << std::endl;
    std::cout << GRAY << "  │ " << RESET << BOLD << "memory  " << RESET << GRAY << " Show long-term mem " 
              << "│ " << RESET << BOLD << "exit    " << RESET << GRAY << " Terminate agent   " << "│" << RESET << std::endl;
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
                               skillManager.getSystemPromptAddition() + "\n" +
                               "Current working directory for your tools: " + path + "\n" +
                               "Current system time: " + date_ss.str() + "\n" +
                               "CRITICAL GUIDELINES:\n" +
                               "1. SELF-HEURISTIC THINKING: Before calling any tool, you MUST output your internal reasoning process in the 'content' field. Use this structure:\n" +
                               "   - [THOUGHT]: What do I know? What is missing?\n" +
                               "   - [PLAN]: What are the next 2-3 steps?\n" +
                               "   - [REFLECTION]: Are there risks? Is there a better way?\n" +
                               "2. PROACTIVE INTERACTION: If a task is ambiguous or risky, DO NOT guess. Ask the user for clarification or propose multiple options.\n" +
                               "3. PROJECT MAPPING: Always start by calling 'project_overview' to establish a mental map of the codebase.\n" +
                               "4. ENGINEERING STRATEGY (Entry-Point-to-Call-Chain): To understand logic, start from the main entry point (e.g., main(), app.py). Follow the call chain using 'lsp_definition' (to find implementation) and 'lsp_references' (to find callers). Use 'read_batch_lines' to see multiple links in the chain simultaneously. Avoid reading random files.\n" +
                               "5. TOKEN EFFICIENCY: Reading full content of large files is expensive. Use 'code_ast_analyze' to see file structure first. When you need to examine multiple code locations (even across different files), ALWAYS prefer 'read_batch_lines' over multiple 'read_file_lines' calls to save tokens and maintain logical coherence.\n" +
                               "6. CLOSED-LOOP VERIFICATION: After any modification, you MUST call 'diagnostics_check' to verify the build and 'read_file_lines' to double-check your own work.\n" +
                               "7. ERROR REFLECTION: If a tool fails, analyze the error output, explain why it failed, and adjust your plan accordingly.";
    
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", systemPrompt}});

    std::string userInput;
    while (true) {
        // Update and print status bar before prompt
        size_t currentSize = contextManager.getSize(messages);
        int taskCount = mcpManager.getTotalTaskCount();
        printStatusBar(cfg.llm.model, currentSize, taskCount);

        std::cout << CYAN << BOLD << "You > " << RESET;
        if (!std::getline(std::cin, userInput) || userInput == "exit") break;
        if (userInput.empty()) continue;
        
        if (userInput == "clear") {
            messages = nlohmann::json::array();
            messages.push_back({{"role", "system"}, {"content", systemPrompt}});
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
                fs::path backupPath = fs::u8path(path) / ".photon" / "backups" / fs::u8path(lastFile);
                if (!fs::exists(backupPath)) {
                    std::cout << RED << "✖ No backup found for: " << lastFile << RESET << std::endl;
                } else {
                    std::cout << YELLOW << BOLD << "Reverting changes in: " << RESET << lastFile << std::endl;
                    
                    // Show Diff before undo
                    std::ifstream bFile(backupPath);
                    std::string bContent((std::istreambuf_iterator<char>(bFile)), std::istreambuf_iterator<char>());
                    bFile.close();
                    
                    showGitDiff(lastFile, bContent);

                    while (true) {
                        std::cout << YELLOW << BOLD << "⚠  CONFIRMATION: " << RESET 
                                  << "Revert to previous backup? [y/N/v (view full)] " << std::flush;
                        std::string confirm;
                        std::getline(std::cin, confirm);
                        std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
                        
                        if (confirm == "v") {
                            showGitDiff(lastFile, bContent, true);
                            continue;
                        }

                        if (confirm == "y" || confirm == "yes") {
                            nlohmann::json result = mcpManager.callTool("builtin", "file_undo", {{"path", lastFile}});
                            if (result.contains("content")) {
                                std::cout << GREEN << "✔ " << result["content"][0]["text"].get<std::string>() << RESET << std::endl;
                                // Feedback to the agent
                                messages.push_back({{"role", "user"}, {"content", "[SYSTEM]: User has undone your last change to " + lastFile + ". Please reflect on why the change was reverted and try a different approach if necessary."}});
                            } else {
                                std::cout << RED << "✖ Undo failed: " << result.dump() << RESET << std::endl;
                            }
                        } else {
                            std::cout << GRAY << "Undo cancelled." << RESET << std::endl;
                        }
                        break;
                    }
                }
            }
            continue;
        }

        messages.push_back({{"role", "user"}, {"content", userInput}});

        bool continues = true;
        int iteration = 0;
        int max_iterations = 50; // Increased default limit to 50 rounds

        while (continues && iteration < max_iterations) {
            iteration++;
            // Apply context management
            messages = contextManager.manage(messages);

            // Update status bar during thinking
            printStatusBar(cfg.llm.model, contextManager.getSize(messages), mcpManager.getTotalTaskCount());

            std::cout << YELLOW << "Thinking (Step " << iteration << "/" << max_iterations << ")..." << RESET << "\r" << std::flush;
            nlohmann::json response = llmClient->chatWithTools(messages, llmTools);
            
            if (response.is_null() || !response.contains("choices") || response["choices"].empty()) {
                break;
            }

            auto& choice = response["choices"][0];
            auto& message = choice["message"];
            
            // Add assistant's message to the conversation history
            messages.push_back(message);

            // 1. Always display content if present (this is the "Thinking" or "Reasoning" part)
            if (message.contains("content") && !message["content"].is_null()) {
                std::string content = message["content"].get<std::string>();
                if (!content.empty()) {
                    std::cout << "           " << "\r"; // Clear "Thinking..."
                    std::cout << GRAY << BOLD << "🤔 [Thought] " << RESET << renderMarkdown(content) << std::endl;
                }
            }

            // 2. Handle Tool Calls
            if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                for (auto& toolCall : message["tool_calls"]) {
                    std::string fullName = toolCall["function"]["name"];
                    std::string argsStr = toolCall["function"]["arguments"];
                    nlohmann::json args;
                    try {
                        args = nlohmann::json::parse(argsStr);
                    } catch (...) {
                        args = nlohmann::json::object();
                    }

                    // Split server_name__tool_name
                    size_t pos = fullName.find("__");
                    if (pos != std::string::npos) {
                        std::string serverName = fullName.substr(0, pos);
                        std::string toolName = fullName.substr(pos + 2);

                        std::cout << YELLOW << BOLD << "⚙️  [Action] " << RESET 
                                  << CYAN << serverName << RESET << "::" << BOLD << toolName << RESET 
                                  << " " << GRAY << args.dump() << RESET << std::endl;
                        
                        // Human-in-the-loop: Confirmation for risky tools
                        if (isRiskyTool(toolName)) {
                            // Specialized Diff Preview for file operations
                            if (toolName == "file_write" && args.contains("content") && args.contains("path")) {
                                showGitDiff(args["path"], args["content"]);
                            } else if (toolName == "file_edit_lines" && args.contains("path")) {
                                // For file_edit_lines, we need to simulate the result for the diff
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
                                    
                                    if (start >= 1 && start <= (int)lines.size() + 1) {
                                        std::vector<std::string> newLines;
                                        std::istringstream iss(content);
                                        std::string nl;
                                        while (std::getline(iss, nl)) newLines.push_back(nl);

                                        if (op == "replace" && end >= start && end <= (int)lines.size()) {
                                            lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                            lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                        } else if (op == "insert") {
                                            lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                        } else if (op == "delete" && end >= start && end <= (int)lines.size()) {
                                            lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                        }

                                        std::string simulated;
                                        for (size_t i = 0; i < lines.size(); ++i) {
                                            simulated += lines[i] + (i == lines.size() - 1 ? "" : "\n");
                                        }
                                        showGitDiff(path, simulated);
                                    }
                                }
                            }

                            while (true) {
                                std::cout << YELLOW << BOLD << "⚠  CONFIRMATION REQUIRED: " << RESET 
                                          << "Proceed? [y/N/v (view full)] " << std::flush;
                                std::string confirm;
                                std::getline(std::cin, confirm);
                                std::transform(confirm.begin(), confirm.end(), confirm.begin(), ::tolower);
                                
                                if (confirm == "v") {
                                    // Show full diff
                                    if (toolName == "file_write") {
                                        showGitDiff(args["path"], args["content"], true);
                                    } else if (toolName == "file_edit_lines") {
                                        // Re-run simulation and show full diff
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
                                            if (op == "replace" && end >= start && end <= (int)lines.size()) {
                                                lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                                lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                            } else if (op == "insert") {
                                                lines.insert(lines.begin() + start - 1, newLines.begin(), newLines.end());
                                            } else if (op == "delete" && end >= start && end <= (int)lines.size()) {
                                                lines.erase(lines.begin() + start - 1, lines.begin() + end);
                                            }
                                            std::string simulated;
                                            for (size_t i = 0; i < lines.size(); ++i) simulated += lines[i] + (i == lines.size() - 1 ? "" : "\n");
                                            showGitDiff(path, simulated, true);
                                        }
                                    }
                                    continue; // Ask again
                                }

                                if (confirm != "y" && confirm != "yes") {
                                    std::cout << RED << "✖  Action cancelled by user." << RESET << std::endl;
                                    messages.push_back({
                                        {"role", "tool"},
                                        {"tool_call_id", toolCall["id"]},
                                        {"name", fullName},
                                        {"content", "{\"error\": \"Action cancelled by user.\"}"}
                                    });
                                    continues = false; // Stop the loop for this turn
                                    break;
                                }
                                break; // confirmed 'y'
                            }
                            if (!continues) break; // Break out of tool call loop if cancelled
                        }

                        nlohmann::json result = mcpManager.callTool(serverName, toolName, args);
                        
                        // Add tool result to messages
                        messages.push_back({
                            {"role", "tool"},
                            {"tool_call_id", toolCall["id"]},
                            {"name", fullName},
                            {"content", result.dump()}
                        });
                    }
                }
                continues = true;
            } else {
                // No more tool calls, we are done
                continues = false;
            }

            if (iteration >= max_iterations && continues) {
                std::cout << YELLOW << BOLD << "\n⚠  LIMIT REACHED: " << RESET 
                          << "Reached maximum thinking steps (" << max_iterations << "). Continue for 20 more steps? [y/N] " << std::flush;
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

        // Final Context Usage Display
        std::cout << CYAN << "── Memory: " << BOLD << contextManager.getSize(messages) << RESET << CYAN << " chars ──" << RESET << std::endl;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    } catch (const std::exception& e) {
        std::cerr << "\n" << RED << BOLD << "█ FATAL ERROR: " << RESET << e.what() << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "\n" << RED << BOLD << "█ UNKNOWN FATAL ERROR" << RESET << std::endl;
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    return 0;
}
