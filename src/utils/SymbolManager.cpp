#include "utils/SymbolManager.h"
#include "utils/TreeSitterSymbolProvider.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>
#include <nlohmann/json.hpp>
#include <chrono>
#include <unordered_set>

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

SymbolManager::SymbolManager(const std::string& root) : rootPath(root) {
    loadIndex();
}

SymbolManager::~SymbolManager() {
    if (scanThread.joinable()) {
        scanThread.join();
    }
}

void SymbolManager::registerProvider(std::unique_ptr<ISymbolProvider> provider) {
    if (!provider) return;
    std::lock_guard<std::mutex> lock(mtx);
    providers.push_back(std::move(provider));
}

void SymbolManager::startAsyncScan() {
    if (scanning) return;
    scanning = true;
    scanThread = std::thread(&SymbolManager::performScan, this);
}

void SymbolManager::performScan() {
    try {
        std::vector<Symbol> localSymbols;
        fs::path root(rootPath);
        std::unordered_set<std::string> seenFiles;
        
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                if (shouldIgnore(entry.path())) continue;
                scanFile(entry.path(), localSymbols);

                std::string relPath = fs::relative(entry.path(), root).generic_string();
                seenFiles.insert(relPath);
            }
        }

        std::lock_guard<std::mutex> lock(mtx);
        // Remove deleted files from index
        for (auto it = fileSymbols.begin(); it != fileSymbols.end(); ) {
            if (seenFiles.find(it->first) == seenFiles.end()) {
                fileMeta.erase(it->first);
                it = fileSymbols.erase(it);
            } else {
                ++it;
            }
        }
        symbols = std::move(localSymbols);
    } catch (...) {}
    saveIndex();
    scanning = false;
}

void SymbolManager::scanFile(const fs::path& filePath, std::vector<Symbol>& localSymbols) {
    std::string ext = filePath.extension().string();
    std::string relPath = fs::relative(filePath, fs::path(rootPath)).generic_string();
    bool supported = false;
    std::vector<ISymbolProvider*> treeProviders;
    std::vector<ISymbolProvider*> fallbackProviders;
    {
        std::lock_guard<std::mutex> lock(mtx);
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
    }
    if (!supported) return;
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
        std::lock_guard<std::mutex> lock(mtx);
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
    for (const auto* provider : primaryProviders) {
        auto extracted = provider->extractSymbols(content, relPath);
        extractedAll.insert(extractedAll.end(), extracted.begin(), extracted.end());
    }
    if (fallbackOnEmpty && extractedAll.empty() && !secondaryProviders.empty() && secondaryProviders != primaryProviders) {
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
    {
        std::lock_guard<std::mutex> lock(mtx);
        fileSymbols[relPath] = extractedAll;
        fileMeta[relPath] = meta;
    }
    localSymbols.insert(localSymbols.end(), extractedAll.begin(), extractedAll.end());
}

fs::path SymbolManager::getIndexPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "symbols.json";
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

        std::lock_guard<std::mutex> lock(mtx);
        symbols = std::move(loaded);
        fileSymbols = std::move(loadedFileSymbols);
        fileMeta = std::move(loadedMeta);
    } catch (...) {}
}

void SymbolManager::saveIndex() {
    try {
        std::vector<Symbol> snapshot;
        std::unordered_map<std::string, std::vector<Symbol>> snapshotFileSymbols;
        std::unordered_map<std::string, FileMeta> snapshotMeta;
        {
            std::lock_guard<std::mutex> lock(mtx);
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
    } catch (...) {}
}

std::vector<SymbolManager::Symbol> SymbolManager::search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mtx);
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
    std::lock_guard<std::mutex> lock(mtx);
    auto it = fileSymbols.find(relPath);
    if (it == fileSymbols.end()) return {};
    return it->second;
}

bool SymbolManager::shouldIgnore(const fs::path& path) {
    std::string p = path.string();
    return p.find("node_modules") != std::string::npos || 
           p.find(".git") != std::string::npos || 
           p.find("build") != std::string::npos ||
           p.find(".venv") != std::string::npos;
}
