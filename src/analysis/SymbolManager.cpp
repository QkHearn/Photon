#include "analysis/SymbolManager.h"
#include "analysis/providers/TreeSitterSymbolProvider.h"
#include "analysis/LSPClient.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <unordered_set>
#include <limits>

namespace {
std::uint64_t fnv1a64(const std::string& data) {
    const std::uint64_t offset = 1469598103934665603ull;
    const std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    for (unsigned char c : data) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= prime;
    }
    return hash;
}
} // namespace

static std::string makeSymbolKey(const Symbol& s) {
    return s.path + ":" + std::to_string(s.line) + ":" + s.name;
}

static void decCount(std::unordered_map<std::string, int>& counts, const std::string& key, int delta) {
    auto it = counts.find(key);
    if (it == counts.end()) return;
    it->second -= delta;
    if (it->second <= 0) counts.erase(it);
}

static std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

SymbolManager::SymbolManager(const std::string& root) : rootPath(root) {
    loadIndex();
}

SymbolManager::~SymbolManager() {
    stopWatching();
    if (scanThread.joinable()) {
        scanThread.join();
    }
}

void SymbolManager::setLSPClients(const std::unordered_map<std::string, LSPClient*>& byExt, LSPClient* fallback) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    lspByExtension = byExt;
    lspFallback = fallback;
}

void SymbolManager::registerProvider(std::unique_ptr<ISymbolProvider> provider) {
    if (!provider) return;
    std::unique_lock<std::shared_mutex> lock(mtx);
    providers.push_back(std::move(provider));
}

void SymbolManager::startAsyncScan() {
    static bool enableDebugLog = std::getenv("PHOTON_DEBUG_SCAN") != nullptr;
    
    if (scanning) {
        if (enableDebugLog) {
            std::cout << "[SymbolManager] Scan already in progress, skipping" << std::endl;
        }
        return;
    }
    if (enableDebugLog) {
        std::cout << "[SymbolManager] Starting async scan thread" << std::endl;
    }
    scanning = true;
    scanThread = std::thread(&SymbolManager::performScan, this);
}

void SymbolManager::performScan() {
    static bool enableDebugLog = std::getenv("PHOTON_DEBUG_SCAN") != nullptr;
    
    if (enableDebugLog) {
        std::cout << "[SymbolManager] Starting full scan of: " << rootPath << std::endl;
    }
    
    int fileCount = 0;
    int scannedCount = 0;
    int reusedCount = 0;
    int ignoredCount = 0;
    
    try {
        std::vector<Symbol> localSymbols;
        std::unordered_map<std::string, std::vector<Symbol>> localFileSymbols;
        fs::path root(rootPath);
        std::unordered_set<std::string> seenFiles;
        
        if (enableDebugLog) {
            std::cout << "[SymbolManager] Providers registered: " << providers.size() << std::endl;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                fileCount++;
                if (shouldIgnore(entry.path())) {
                    ignoredCount++;
                    continue;
                }
                
                std::string relPath = fs::relative(entry.path(), root).generic_string();
                
                // 增量：未修改的文件直接复用索引，不读文件、不解析
                std::uintmax_t fsize = 0;
                std::time_t fmtime = 0;
                try {
                    fsize = fs::file_size(entry.path());
                    auto ftime = fs::last_write_time(entry.path());
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    fmtime = std::chrono::system_clock::to_time_t(sctp);
                } catch (...) {}
                
                bool reused = false;
                {
                    std::shared_lock<std::shared_mutex> lock(mtx);
                    auto itMeta = fileMeta.find(relPath);
                    auto itSyms = fileSymbols.find(relPath);
                    if (itMeta != fileMeta.end() && itSyms != fileSymbols.end()
                        && itMeta->second.size == fsize && itMeta->second.mtime == fmtime) {
                        const auto& syms = itSyms->second;
                        if (!syms.empty()) {
                            localFileSymbols[relPath] = syms;
                            localSymbols.insert(localSymbols.end(), syms.begin(), syms.end());
                        }
                        seenFiles.insert(relPath);
                        reusedCount++;
                        reused = true;
                    }
                }
                if (reused) continue;
                
                if (enableDebugLog && scannedCount < 10) {
                    std::string ext = entry.path().extension().string();
                    std::cout << "[SymbolManager] Scanning: " << entry.path().string() << " (ext: " << ext << ")" << std::endl;
                }
                
                std::vector<Symbol> fileSyms;
                scanFile(entry.path(), fileSyms);
                
                if (!fileSyms.empty()) {
                    localFileSymbols[relPath] = std::move(fileSyms);
                    localSymbols.insert(localSymbols.end(), localFileSymbols[relPath].begin(), localFileSymbols[relPath].end());
                }
                
                scannedCount++;
                seenFiles.insert(relPath);
            }
        }

        // 计算需要删除的文件（不持有锁）
        std::vector<std::string> filesToRemove;
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            for (const auto& [path, _] : fileSymbols) {
                if (seenFiles.find(path) == seenFiles.end()) {
                    filesToRemove.push_back(path);
                }
            }
        }
        
        // 一次性更新所有数据（持有锁时间最短）
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            
            // 删除旧文件
            for (const auto& path : filesToRemove) {
                fileMeta.erase(path);
                fileSymbols.erase(path);
            }
            
            // 更新新文件
            for (auto& [path, syms] : localFileSymbols) {
                fileSymbols[path] = std::move(syms);
            }
            
            // 更新全局符号列表
            symbols = std::move(localSymbols);
        }
        
        // 只在调试模式下显示扫描摘要
        if (enableDebugLog) {
            std::cout << "[SymbolManager] Scan complete: " << fileCount << " files, "
                      << reusedCount << " reused (unchanged), " << scannedCount << " parsed, "
                      << ignoredCount << " ignored, " << symbols.size() << " symbols" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SymbolManager] Scan failed with exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[SymbolManager] Scan failed with unknown exception" << std::endl;
    }
    saveIndex();
    if (enableDebugLog) {
        std::cout << "[SymbolManager] Index saved" << std::endl;
    }
    scanning = false;
}

void SymbolManager::startWatching(int intervalSeconds) {
    if (watching) return;
    watchInterval = intervalSeconds;
    watching = true;
    stopWatch = false;
    watchThread = std::thread(&SymbolManager::watchLoop, this);
}

void SymbolManager::stopWatching() {
    if (!watching) return;
    stopWatch = true;
    watching = false;
    if (watchThread.joinable()) {
        watchThread.join();
    }
}

void SymbolManager::watchLoop() {
    while (!stopWatch) {
        std::this_thread::sleep_for(std::chrono::seconds(watchInterval));
        if (stopWatch) break;
        if (!scanning) {
            checkFileChanges();
        }
    }
}

void SymbolManager::checkFileChanges() {
    try {
        fs::path root(rootPath);
        std::unordered_set<std::string> currentFiles;
        std::vector<fs::path> filesToUpdate;
        
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                if (shouldIgnore(entry.path())) continue;
                std::string relPath = fs::relative(entry.path(), root).generic_string();
                currentFiles.insert(relPath);
                
                FileMeta currentMeta;
                try {
                    currentMeta.size = fs::file_size(entry.path());
                    auto ftime = fs::last_write_time(entry.path());
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    currentMeta.mtime = std::chrono::system_clock::to_time_t(sctp);
                } catch (...) {
                    continue;
                }
                
                bool needsUpdate = false;
                {
                    std::unique_lock<std::shared_mutex> lock(mtx);
                    auto it = fileMeta.find(relPath);
                    if (it != fileMeta.end()) {
                        if (it->second.mtime != currentMeta.mtime || it->second.size != currentMeta.size) {
                            needsUpdate = true;
                        }
                    } else {
                        needsUpdate = true;
                    }
                }
                
                if (needsUpdate) {
                    filesToUpdate.push_back(entry.path());
                }
            }
        }
        
        // 批量扫描（不持有锁）
        std::unordered_map<std::string, std::vector<Symbol>> updatedFiles;
        for (const auto& filePath : filesToUpdate) {
            std::string relPath = fs::relative(filePath, root).generic_string();
            std::vector<Symbol> fileSyms;
            scanFile(filePath, fileSyms);
            updatedFiles[relPath] = std::move(fileSyms);
        }
        
        // 计算删除的文件
        std::vector<std::string> filesToRemove;
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            for (const auto& [path, _] : fileSymbols) {
                if (currentFiles.find(path) == currentFiles.end()) {
                    filesToRemove.push_back(path);
                }
            }
        }
        
        // 一次性更新（持有锁时间最短）
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            
            // 删除旧符号
            for (const auto& path : filesToRemove) {
                fileMeta.erase(path);
                fileSymbols.erase(path);
                symbols.erase(
                    std::remove_if(symbols.begin(), symbols.end(),
                        [&](const Symbol& s) { return s.path == path; }),
                    symbols.end());
            }
            
            // 更新新符号
            for (auto& pair : updatedFiles) {
                const std::string& filePath = pair.first;
                auto& syms = pair.second;
                symbols.erase(
                    std::remove_if(symbols.begin(), symbols.end(),
                        [&filePath](const Symbol& s) { return s.path == filePath; }),
                    symbols.end());
                if (!syms.empty()) {
                    fileSymbols[filePath] = syms;
                    symbols.insert(symbols.end(), syms.begin(), syms.end());
                }
            }
        }
        
        if (!filesToUpdate.empty() || !filesToRemove.empty()) {
            saveIndex();
        }
    } catch (...) {}
}

void SymbolManager::updateFile(const std::string& relPath) {
    fs::path fullPath = fs::path(rootPath) / fs::u8path(relPath);
    if (fs::exists(fullPath) && !shouldIgnore(fullPath)) {
        updateSingleFile(fullPath);
        saveIndex();
    }
}

void SymbolManager::updateSingleFile(const fs::path& filePath) {
    std::string relPath = fs::relative(filePath, fs::path(rootPath)).generic_string();
    std::vector<Symbol> newSymbols;
    scanFile(filePath, newSymbols);
    
    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        symbols.erase(
            std::remove_if(symbols.begin(), symbols.end(),
                [&](const Symbol& s) { return s.path == relPath; }),
            symbols.end());
        if (!newSymbols.empty()) {
            fileSymbols[relPath] = newSymbols;
            symbols.insert(symbols.end(), newSymbols.begin(), newSymbols.end());
        } else {
            fileSymbols.erase(relPath);
        }
    }
}

void SymbolManager::scanFile(const fs::path& filePath, std::vector<Symbol>& localSymbols) {
    std::string ext = filePath.extension().string();
    std::string relPath = fs::relative(filePath, fs::path(rootPath)).generic_string();
    bool supported = false;
    std::vector<ISymbolProvider*> treeProviders;
    std::vector<ISymbolProvider*> fallbackProviders;
    std::unordered_map<std::string, LSPClient*> lspByExtSnapshot;
    LSPClient* lspFallbackSnapshot = nullptr;
    
    static bool enableDebugLog = std::getenv("PHOTON_DEBUG_SCAN") != nullptr;
    
    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        for (const auto& provider : providers) {
            if (provider->supportsExtension(ext)) {
                supported = true;
                if (dynamic_cast<TreeSitterSymbolProvider*>(provider.get()) != nullptr) {
                    treeProviders.push_back(provider.get());
                } else {
                    fallbackProviders.push_back(provider.get());
                }
            }
        }
        lspByExtSnapshot = lspByExtension;
        lspFallbackSnapshot = lspFallback;
    }
    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
    bool lspSupported = (lspByExtSnapshot.find(extLower) != lspByExtSnapshot.end()) || (lspFallbackSnapshot != nullptr);
    
    // 策略：只依赖 providers 决定是否扫描
    // LSP 仅用于符号提取的回退，不影响扫描决策
    if (!supported) {
        return;
    }
    const std::vector<ISymbolProvider*>& primaryProviders =
        !treeProviders.empty() ? treeProviders : fallbackProviders;
    const std::vector<ISymbolProvider*>& secondaryProviders =
        (!treeProviders.empty() && !fallbackProviders.empty()) ? fallbackProviders : treeProviders;

    FileMeta meta;
    try {
        meta.size = fs::file_size(filePath);
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        meta.mtime = std::chrono::system_clock::to_time_t(sctp);
    } catch (...) {}

    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    meta.hash = fnv1a64(content);

    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        auto itMeta = fileMeta.find(relPath);
        auto itSymbols = fileSymbols.find(relPath);
        if (itMeta != fileMeta.end() && itSymbols != fileSymbols.end()) {
            if (itMeta->second.hash == meta.hash) {
                localSymbols.insert(localSymbols.end(), itSymbols->second.begin(), itSymbols->second.end());
                return;
            }
        }
    }

    std::vector<Symbol> extractedAll;
    auto pickLsp = [&](const std::string& extLower) -> LSPClient* {
        if (!lspByExtSnapshot.empty()) {
            auto it = lspByExtSnapshot.find(extLower);
            if (it != lspByExtSnapshot.end()) return it->second;
        }
        return lspFallbackSnapshot;
    };
    auto mapKindToType = [](int kind) -> std::string {
        switch (kind) {
            case 5: return "class";
            case 6: return "method";
            case 10: return "enum";
            case 11: return "interface";
            case 12: return "function";
            case 23: return "struct";
            default: return "symbol";
        }
    };

    // 优先使用 Tree-sitter/Regex providers（更可靠）
    // LSP 可能在扫描时还未完全初始化，导致返回空结果
    for (const auto* provider : primaryProviders) {
        auto extracted = provider->extractSymbols(content, relPath);
        extractedAll.insert(extractedAll.end(), extracted.begin(), extracted.end());
    }
    
    // 如果 providers 没有提取到符号，尝试使用 LSP（作为回退）
    if (extractedAll.empty()) {
        LSPClient* lsp = pickLsp(extLower);
        if (lsp) {
            std::string fileUri = "file://" + fs::absolute(filePath).u8string();
            auto docSymbols = lsp->documentSymbols(fileUri);
            for (const auto& ds : docSymbols) {
                Symbol s;
                s.name = ds.name;
                s.type = mapKindToType(ds.kind);
                s.source = "lsp";
                s.path = relPath;
                s.line = ds.range.start.line + 1;
                s.endLine = ds.range.end.line + 1;
                s.signature = ds.detail;
                extractedAll.push_back(std::move(s));
            }
        }
    }
    if (extractedAll.empty() && !secondaryProviders.empty() && secondaryProviders != primaryProviders) {
        for (const auto* provider : secondaryProviders) {
            auto extracted = provider->extractSymbols(content, relPath);
            extractedAll.insert(extractedAll.end(), extracted.begin(), extracted.end());
        }
    }
    if (!extractedAll.empty()) {
        std::unordered_set<std::string> seen;
        std::vector<Symbol> unique;
        unique.reserve(extractedAll.size());
        for (auto& s : extractedAll) {
            std::string key = s.type + "|" + s.name + "|" + s.source + "|" + s.path + "|" +
                              std::to_string(s.line) + "|" + s.signature;
            if (seen.insert(key).second) {
                unique.push_back(std::move(s));
            }
        }
        extractedAll.swap(unique);
    }
    std::vector<std::pair<std::string, std::vector<CallInfo>>> removedCalls;
    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        fileSymbols[relPath] = extractedAll;
        fileMeta[relPath] = meta;
        for (auto it = symbolCalls.begin(); it != symbolCalls.end(); ) {
            if (it->first.rfind(relPath + ":", 0) == 0) {
                removedCalls.push_back(*it);
                it = symbolCalls.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = callGraphAdj.begin(); it != callGraphAdj.end(); ) {
            if (it->first.rfind(relPath + ":", 0) == 0) {
                it = callGraphAdj.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& item : removedCalls) {
            const auto& key = item.first;
            const auto& calls = item.second;
            callerOutCounts.erase(key);
            for (const auto& c : calls) {
                decCount(calleeCounts, c.name, 1);
            }
        }
    }

    std::unordered_map<std::string, std::vector<Symbol>> globalNameIndex;
    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        for (const auto& s : symbols) {
            globalNameIndex[s.name].push_back(s);
        }
    }
    std::unordered_map<std::string, std::vector<Symbol>> localNameIndex;
    for (const auto& s : extractedAll) {
        localNameIndex[s.name].push_back(s);
    }
    auto resolveByName = [&](const std::string& name) -> std::string {
        auto tryResolve = [&](const std::string& keyName) -> std::string {
            auto localIt = localNameIndex.find(keyName);
            if (localIt != localNameIndex.end() && localIt->second.size() == 1) {
                return makeSymbolKey(localIt->second[0]);
            }
            auto globalIt = globalNameIndex.find(keyName);
            if (globalIt != globalNameIndex.end() && globalIt->second.size() == 1) {
                return makeSymbolKey(globalIt->second[0]);
            }
            return "";
        };

        std::string direct = tryResolve(name);
        if (!direct.empty()) return direct;

        auto stripQualifier = [](const std::string& n) -> std::string {
            size_t pos = n.rfind("::");
            if (pos != std::string::npos) return n.substr(pos + 2);
            pos = n.rfind('.');
            if (pos != std::string::npos) return n.substr(pos + 1);
            return n;
        };
        std::string baseName = stripQualifier(name);
        if (baseName != name) {
            std::string base = tryResolve(baseName);
            if (!base.empty()) return base;
        }

        auto localIt = localNameIndex.find(name);
        if (localIt != localNameIndex.end() && localIt->second.size() == 1) {
            return makeSymbolKey(localIt->second[0]);
        }
        auto globalIt = globalNameIndex.find(name);
        if (globalIt != globalNameIndex.end() && globalIt->second.size() == 1) {
            return makeSymbolKey(globalIt->second[0]);
        }
        std::string lowerName = toLowerStr(name);
        std::vector<Symbol> caseMatches;
        for (const auto& pair : globalNameIndex) {
            if (toLowerStr(pair.first) == lowerName) {
                for (const auto& s : pair.second) caseMatches.push_back(s);
            }
        }
        if (caseMatches.empty() && baseName != name) {
            std::string lowerBase = toLowerStr(baseName);
            for (const auto& pair : globalNameIndex) {
                if (toLowerStr(pair.first) == lowerBase) {
                    for (const auto& s : pair.second) caseMatches.push_back(s);
                }
            }
        }
        if (caseMatches.size() == 1) return makeSymbolKey(caseMatches[0]);
        if (!caseMatches.empty()) return "ambiguous:" + name;
        return "unresolved:" + name;
    };

    auto resolveByLsp = [&](const CallInfo& call) -> std::string {
        std::string extLower = toLowerStr(filePath.extension().string());
        LSPClient* lsp = pickLsp(extLower);
        if (!lsp) return "";
        std::string fileUri = "file://" + fs::absolute(filePath).u8string();
        LSPClient::Position pos{call.line - 1, call.character};
        auto defs = lsp->goToDefinition(fileUri, pos);
        if (defs.empty()) return "";
        std::vector<LSPClient::Location> ranked;
        ranked.reserve(defs.size());
        for (const auto& loc : defs) ranked.push_back(loc);
        std::string callerRel = relPath;
        std::stable_sort(ranked.begin(), ranked.end(), [&](const LSPClient::Location& a, const LSPClient::Location& b) {
            std::string ap = LSPClient::uriToPath(a.uri);
            std::string bp = LSPClient::uriToPath(b.uri);
            if (ap.empty() || bp.empty()) return ap < bp;
            fs::path ar = fs::u8path(ap);
            fs::path br = fs::u8path(bp);
            if (ar.is_absolute()) {
                try { ar = fs::relative(ar, fs::path(rootPath)); } catch (...) {}
            }
            if (br.is_absolute()) {
                try { br = fs::relative(br, fs::path(rootPath)); } catch (...) {}
            }
            std::string arl = ar.generic_string();
            std::string brl = br.generic_string();
            bool aSameFile = (arl == callerRel);
            bool bSameFile = (brl == callerRel);
            if (aSameFile != bSameFile) return aSameFile;
            return arl < brl;
        });

        for (const auto& loc : ranked) {
            std::string targetPath = LSPClient::uriToPath(loc.uri);
            if (targetPath.empty()) continue;
            fs::path relPath = fs::u8path(targetPath);
            if (relPath.is_absolute()) {
                try { relPath = fs::relative(relPath, fs::path(rootPath)); } catch (...) {}
            }
            std::string rel = relPath.generic_string();
            int line = loc.range.start.line + 1;
            auto target = findEnclosingSymbol(rel, line);
            if (target.has_value()) return makeSymbolKey(target.value());
        }
        return "";
    };

    auto resolveCalleeCall = [&](const CallInfo& call) -> std::string {
        std::string nameKey = resolveByName(call.name);
        if (nameKey.rfind("ambiguous:", 0) == 0 || nameKey.rfind("unresolved:", 0) == 0) {
            std::string lspKey = resolveByLsp(call);
            if (!lspKey.empty()) return lspKey;
        }
        return nameKey;
    };

    for (const auto& s : extractedAll) {
        if (s.line <= 0 || s.endLine <= 0) continue;
        auto calls = extractCalls(relPath, s.line, s.endLine);
        if (!calls.empty()) {
            std::unordered_set<std::string> uniq;
            for (const auto& c : calls) {
                uniq.insert(resolveCalleeCall(c));
            }
            std::unique_lock<std::shared_mutex> lock(mtx);
            std::string key = makeSymbolKey(s);
            symbolCalls[key] = calls;
            callerOutCounts[key] = static_cast<int>(calls.size());
            for (const auto& c : calls) {
                calleeCounts[c.name] += 1;
            }
            callGraphAdj[key] = std::vector<std::string>(uniq.begin(), uniq.end());
        }
    }
    localSymbols.insert(localSymbols.end(), extractedAll.begin(), extractedAll.end());
}

fs::path SymbolManager::getIndexPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "symbols.json";
}

fs::path SymbolManager::getCallIndexPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "symbol_calls.json";
}

fs::path SymbolManager::getCallGraphPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "call_graph.json";
}

void SymbolManager::loadIndex() {
    try {
        fs::path indexPath = getIndexPath();
        if (!fs::exists(indexPath)) return;

        std::ifstream file(indexPath);
        if (!file.is_open()) return;

        nlohmann::json j;
        file >> j;
        std::vector<Symbol> loaded;
        std::unordered_map<std::string, std::vector<Symbol>> loadedFileSymbols;
        std::unordered_map<std::string, FileMeta> loadedMeta;

        if (j.is_array()) {
            for (const auto& item : j) {
                Symbol s;
                s.name = item.value("name", "");
                s.type = item.value("type", "");
                s.path = item.value("path", "");
                s.line = item.value("line", 0);
                s.endLine = item.value("endLine", 0);
                s.signature = item.value("signature", "");
                s.source = item.value("source", "legacy");
                if (!s.name.empty() && !s.path.empty()) {
                    loaded.push_back(std::move(s));
                }
            }
        } else if (j.is_object() && j.value("version", 0) >= 2 && j.contains("files")) {
            auto files = j["files"];
            for (auto it = files.begin(); it != files.end(); ++it) {
                const std::string relPath = it.key();
                const auto& entry = it.value();
                FileMeta meta;
                if (entry.contains("meta")) {
                    meta.size = entry["meta"].value("size", 0);
                    meta.mtime = entry["meta"].value("mtime", 0);
                    meta.hash = entry["meta"].value("hash", 0);
                }
                std::vector<Symbol> fileSyms;
                if (entry.contains("symbols") && entry["symbols"].is_array()) {
                    for (const auto& item : entry["symbols"]) {
                        Symbol s;
                        s.name = item.value("name", "");
                        s.type = item.value("type", "");
                        s.path = item.value("path", relPath);
                        s.line = item.value("line", 0);
                        s.endLine = item.value("endLine", 0);
                        s.signature = item.value("signature", "");
                        s.source = item.value("source", "legacy");
                        if (!s.name.empty() && !s.path.empty()) {
                            fileSyms.push_back(std::move(s));
                        }
                    }
                }
                if (!fileSyms.empty()) {
                    loadedFileSymbols[relPath] = std::move(fileSyms);
                    loadedMeta[relPath] = meta;
                }
            }
            for (const auto& pair : loadedFileSymbols) {
                loaded.insert(loaded.end(), pair.second.begin(), pair.second.end());
            }
        }

        std::unique_lock<std::shared_mutex> lock(mtx);
        symbols = std::move(loaded);
        fileSymbols = std::move(loadedFileSymbols);
        fileMeta = std::move(loadedMeta);
    } catch (...) {}
    loadCallIndex();
    loadCallGraph();
}

void SymbolManager::saveIndex() {
    try {
        std::vector<Symbol> snapshot;
        std::unordered_map<std::string, std::vector<Symbol>> snapshotFileSymbols;
        std::unordered_map<std::string, FileMeta> snapshotMeta;
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            snapshot = symbols;
            snapshotFileSymbols = fileSymbols;
            snapshotMeta = fileMeta;
        }

        nlohmann::json files = nlohmann::json::object();
        for (const auto& pair : snapshotFileSymbols) {
            const auto& relPath = pair.first;
            const auto& syms = pair.second;
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : syms) {
                arr.push_back({
                    {"name", s.name},
                    {"type", s.type},
                    {"source", s.source},
                    {"path", s.path},
                    {"line", s.line},
                    {"endLine", s.endLine},
                    {"signature", s.signature}
                });
            }
            FileMeta meta = snapshotMeta[relPath];
            files[relPath] = {
                {"meta", {{"size", meta.size}, {"mtime", meta.mtime}, {"hash", meta.hash}}},
                {"symbols", arr}
            };
        }
        nlohmann::json j = {
            {"version", 2},
            {"files", files}
        };

        fs::path indexPath = getIndexPath();
        fs::create_directories(indexPath.parent_path());
        std::ofstream file(indexPath);
        if (!file.is_open()) return;
        file << j.dump(2);
        file.flush();
        file.close();
    } catch (...) {}
    saveCallIndex();
    saveCallGraph();
}

void SymbolManager::loadCallIndex() {
    try {
        fs::path indexPath = getCallIndexPath();
        if (!fs::exists(indexPath)) return;
        std::ifstream file(indexPath);
        if (!file.is_open()) return;
        nlohmann::json j;
        file >> j;
        if (!j.is_object() || !j.contains("calls")) return;
        std::unordered_map<std::string, std::vector<CallInfo>> loadedCalls;
        for (const auto& item : j["calls"]) {
            std::string key = item.value("key", "");
            if (key.empty() || !item.contains("entries")) continue;
            std::vector<CallInfo> calls;
            for (const auto& c : item["entries"]) {
                CallInfo info;
                info.name = c.value("name", "");
                info.line = c.value("line", 0);
                info.character = c.value("character", 0);
                if (!info.name.empty()) calls.push_back(info);
            }
            if (!calls.empty()) loadedCalls[key] = std::move(calls);
        }
        std::unique_lock<std::shared_mutex> lock(mtx);
        symbolCalls = std::move(loadedCalls);
        calleeCounts.clear();
        callerOutCounts.clear();
        for (const auto& pair : symbolCalls) {
            callerOutCounts[pair.first] = static_cast<int>(pair.second.size());
            for (const auto& c : pair.second) {
                calleeCounts[c.name] += 1;
            }
        }
    } catch (...) {}
}

void SymbolManager::saveCallIndex() {
    try {
        std::unordered_map<std::string, std::vector<CallInfo>> snapshot;
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            snapshot = symbolCalls;
        }
        nlohmann::json calls = nlohmann::json::array();
        for (const auto& pair : snapshot) {
            nlohmann::json entries = nlohmann::json::array();
            for (const auto& c : pair.second) {
                entries.push_back({
                    {"name", c.name},
                    {"line", c.line},
                    {"character", c.character}
                });
            }
            calls.push_back({{"key", pair.first}, {"entries", entries}});
        }
        nlohmann::json root = {
            {"version", 1},
            {"calls", calls}
        };
        fs::path indexPath = getCallIndexPath();
        fs::create_directories(indexPath.parent_path());
        std::ofstream out(indexPath);
        out << root.dump(2);
    } catch (...) {}
}

void SymbolManager::loadCallGraph() {
    try {
        fs::path indexPath = getCallGraphPath();
        if (!fs::exists(indexPath)) return;
        std::ifstream file(indexPath);
        if (!file.is_open()) return;
        nlohmann::json j;
        file >> j;
        if (!j.is_object() || !j.contains("edges")) return;
        std::unordered_map<std::string, std::vector<std::string>> loadedAdj;
        for (const auto& item : j["edges"]) {
            std::string from = item.value("from", "");
            if (from.empty() || !item.contains("to")) continue;
            std::vector<std::string> tos;
            for (const auto& t : item["to"]) {
                if (t.is_string()) tos.push_back(t.get<std::string>());
            }
            if (!tos.empty()) loadedAdj[from] = std::move(tos);
        }
        std::unique_lock<std::shared_mutex> lock(mtx);
        callGraphAdj = std::move(loadedAdj);
    } catch (...) {}
}

void SymbolManager::saveCallGraph() {
    try {
        std::unordered_map<std::string, std::vector<std::string>> snapshot;
        {
            std::unique_lock<std::shared_mutex> lock(mtx);
            snapshot = callGraphAdj;
        }
        nlohmann::json edges = nlohmann::json::array();
        for (const auto& pair : snapshot) {
            edges.push_back({{"from", pair.first}, {"to", pair.second}});
        }
        nlohmann::json root = {
            {"version", 1},
            {"edges", edges}
        };
        fs::path indexPath = getCallGraphPath();
        fs::create_directories(indexPath.parent_path());
        std::ofstream out(indexPath);
        out << root.dump(2);
    } catch (...) {}
}

std::vector<SymbolManager::Symbol> SymbolManager::search(const std::string& query) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    std::vector<Symbol> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& s : symbols) {
        std::string lowerName = s.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        if (lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(s);
        }
    }
    auto sourcePriority = [](const std::string& source) {
        if (source == "tree_sitter") return 0;
        if (source == "regex") return 1;
        return 2;
    };
    std::stable_sort(results.begin(), results.end(), [&](const Symbol& a, const Symbol& b) {
        std::string aName = a.name;
        std::string bName = b.name;
        std::transform(aName.begin(), aName.end(), aName.begin(), ::tolower);
        std::transform(bName.begin(), bName.end(), bName.begin(), ::tolower);
        bool aExact = aName == lowerQuery;
        bool bExact = bName == lowerQuery;
        if (aExact != bExact) return aExact;
        int aSrc = sourcePriority(a.source);
        int bSrc = sourcePriority(b.source);
        if (aSrc != bSrc) return aSrc < bSrc;
        return aName < bName;
    });
    return results;
}

std::vector<SymbolManager::Symbol> SymbolManager::getFileSymbols(const std::string& relPath) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    auto it = fileSymbols.find(relPath);
    if (it == fileSymbols.end()) return {};
    return it->second;
}

bool SymbolManager::tryGetFileSymbols(const std::string& relPath, std::vector<Symbol>& outSymbols) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    
    auto it = fileSymbols.find(relPath);
    if (it == fileSymbols.end()) {
        return false;
    }
    
    outSymbols = it->second;
    return true;
}

std::optional<SymbolManager::Symbol> SymbolManager::findEnclosingSymbol(const std::string& relPath, int line) {
    if (line <= 0) return std::nullopt;
    std::shared_lock<std::shared_mutex> lock(mtx);
    auto it = fileSymbols.find(relPath);
    if (it == fileSymbols.end()) return std::nullopt;

    const auto& symbols = it->second;
    const Symbol* best = nullptr;
    int bestSpan = std::numeric_limits<int>::max();
    int bestStart = -1;

    for (const auto& s : symbols) {
        if (s.line <= 0) continue;
        bool inRange = (s.endLine > 0) ? (s.line <= line && line <= s.endLine) : (s.line <= line);
        if (!inRange) continue;

        int span = (s.endLine > 0) ? (s.endLine - s.line) : std::numeric_limits<int>::max();
        if (span < bestSpan || (span == bestSpan && s.line > bestStart)) {
            best = &s;
            bestSpan = span;
            bestStart = s.line;
        }
    }

    if (!best) return std::nullopt;
    return *best;
}

std::vector<SymbolManager::CallInfo> SymbolManager::getCallsForSymbol(const Symbol& symbol) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto it = symbolCalls.find(makeSymbolKey(symbol));
    if (it == symbolCalls.end()) return {};
    return it->second;
}

int SymbolManager::getGlobalCalleeCount(const std::string& calleeName) const {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto it = calleeCounts.find(calleeName);
    if (it == calleeCounts.end()) return 0;
    return it->second;
}

int SymbolManager::getCallerOutDegree(const Symbol& symbol) const {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto it = callerOutCounts.find(makeSymbolKey(symbol));
    if (it == callerOutCounts.end()) return 0;
    return it->second;
}

std::vector<std::string> SymbolManager::getCalleesForSymbol(const Symbol& symbol) const {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto it = callGraphAdj.find(makeSymbolKey(symbol));
    if (it == callGraphAdj.end()) return {};
    return it->second;
}

std::vector<SymbolManager::CallInfo> SymbolManager::extractCalls(const std::string& relPath, int startLine, int endLine) {
    fs::path fullPath = fs::path(rootPath) / fs::u8path(relPath);
    std::ifstream file(fullPath);
    if (!file.is_open()) return {};
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::vector<CallInfo> allCalls;
    std::unique_lock<std::shared_mutex> lock(mtx);
    for (const auto& provider : providers) {
        if (auto* tsProvider = dynamic_cast<TreeSitterSymbolProvider*>(provider.get())) {
            auto calls = tsProvider->extractCalls(content, relPath, startLine, endLine);
            for (const auto& c : calls) {
                allCalls.push_back({c.name, c.line, c.character});
            }
        }
    }
    return allCalls;
}

bool SymbolManager::shouldIgnore(const fs::path& path) {
    std::string p = path.string();
    
    // 使用配置的忽略模式
    for (const auto& pattern : ignorePatterns) {
        if (p.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    // 如果没有配置，使用默认模式
    if (ignorePatterns.empty()) {
        return p.find("node_modules") != std::string::npos || 
               p.find(".git") != std::string::npos || 
               p.find("build") != std::string::npos ||
               p.find(".venv") != std::string::npos;
    }
    
    return false;
}
