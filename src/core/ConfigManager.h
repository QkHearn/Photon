#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

struct Config {
    struct LLM {
        std::string apiKey;
        std::string baseUrl;
        std::string model;
        std::string systemRole;
    } llm;

    struct Agent {
        size_t contextThreshold;
        std::vector<std::string> fileExtensions;
        bool useBuiltinTools;
        std::string searchApiKey;
        std::vector<std::string> skillRoots; // Directories to search for skills
        bool enableTreeSitter = false;
        bool symbolFallbackOnEmpty = false;
        bool enableLSP = true;  // 是否启用 LSP 功能
        bool enableDebug = false;  // 是否启用调试日志
        std::string lspServerPath;
        std::string lspRootUri;
        struct LSPServer {
            std::string name;
            std::string command;
            std::vector<std::string> extensions;
        };
        std::vector<LSPServer> lspServers;
        struct TreeSitterLanguage {
            std::string name;
            std::vector<std::string> extensions;
            std::string libraryPath;
            std::string symbol;
        };
        std::vector<TreeSitterLanguage> treeSitterLanguages;
        /** 扫描忽略：正则列表（ECMAScript），路径匹配任一则跳过；与 list_project_files 共用。以 . 开头的目录始终不扫描（内置）。字面点用 \\. 如 "\\.git" */
        std::vector<std::string> symbolIgnorePatterns;
    } agent;

    struct MCPServerConfig {
        std::string name;
        std::string command;
    };
    std::vector<MCPServerConfig> mcpServers;

    static Config load(const std::string& pathStr) {
        std::filesystem::path path = std::filesystem::u8path(pathStr);
        std::ifstream f(path);
        if (!f.is_open()) {
            throw std::runtime_error("Could not open config file: " + pathStr);
        }
        
        // Read the file content into a string first to ensure proper encoding handling
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(content);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("JSON Parse Error in " + path.string() + ": " + e.what());
        }
        
        Config cfg;
        cfg.llm.apiKey = j.at("llm").at("api_key").get<std::string>();
        cfg.llm.baseUrl = j.at("llm").at("base_url").get<std::string>();
        cfg.llm.model = j.at("llm").at("model").get<std::string>();
        cfg.llm.systemRole = j.at("llm").at("system_role").get<std::string>();
        
        cfg.agent.contextThreshold = j.at("agent").at("context_threshold").get<size_t>();
        cfg.agent.fileExtensions = j.at("agent").at("file_extensions").get<std::vector<std::string>>();
        cfg.agent.useBuiltinTools = j.at("agent").value("use_builtin_tools", true);
        cfg.agent.searchApiKey = j.at("agent").value("search_api_key", "");
        cfg.agent.enableTreeSitter = j.at("agent").value("enable_tree_sitter", false);
        cfg.agent.symbolFallbackOnEmpty = j.at("agent").value("symbol_fallback_on_empty", false);
        cfg.agent.enableLSP = j.at("agent").value("enable_lsp", true);
        cfg.agent.enableDebug = j.at("agent").value("enable_debug", false);
        cfg.agent.lspServerPath = j.at("agent").value("lsp_server_path", "");
        cfg.agent.lspRootUri = j.at("agent").value("lsp_root_uri", "");
        if (j.at("agent").contains("lsp_servers")) {
            for (const auto& item : j["agent"]["lsp_servers"]) {
                Agent::LSPServer server;
                server.name = item.value("name", "");
                server.command = item.value("command", "");
                server.extensions = item.value("extensions", std::vector<std::string>{});
                if (!server.command.empty()) {
                    cfg.agent.lspServers.push_back(std::move(server));
                }
            }
        }
        if (j.at("agent").contains("skill_roots")) {
            cfg.agent.skillRoots = j.at("agent")["skill_roots"].get<std::vector<std::string>>();
        }
        if (j.at("agent").contains("tree_sitter_languages")) {
            for (const auto& item : j["agent"]["tree_sitter_languages"]) {
                Agent::TreeSitterLanguage lang;
                lang.name = item.value("name", "");
                lang.extensions = item.value("extensions", std::vector<std::string>{});
                lang.libraryPath = item.value("library_path", "");
                lang.symbol = item.value("symbol", "");
                if (!lang.name.empty() && !lang.extensions.empty()) {
                    cfg.agent.treeSitterLanguages.push_back(std::move(lang));
                }
            }
        }
        
        // 读取符号扫描忽略模式
        if (j.at("agent").contains("symbol_ignore_patterns")) {
            cfg.agent.symbolIgnorePatterns = j.at("agent")["symbol_ignore_patterns"].get<std::vector<std::string>>();
        }
        
        if (j.contains("mcp_servers")) {
            for (auto& item : j["mcp_servers"]) {
                cfg.mcpServers.push_back({item["name"], item["command"]});
            }
        }
        
        return cfg;
    }

    void ensurePhotonRules() const {
        std::filesystem::path photonDir = std::filesystem::u8path(".photon");
        if (!std::filesystem::exists(photonDir)) {
            std::filesystem::create_directory(photonDir);
        }
        std::filesystem::path rulesPath = photonDir / "rules";
        std::ofstream f(rulesPath);
        if (f.is_open()) {
            f << "# PhotonRule v1.0\n";
            f << "1. MIN_IO: No full-file reads >500 lines.\n";
            f << "2. PATCH_ONLY: No full-file overwrites.\n";
            f << "3. SEARCH_FIRST: Map symbols before reading.\n";
            f << "4. DECOUPLE: Split files >1000 lines.\n";
            f << "5. JSON_STRICT: Validate schemas.\n";
            f << "6. ASYNC_SAFE: Respect async flows.\n";
            f.close();
        }
    }
};
