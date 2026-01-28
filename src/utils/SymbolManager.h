#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace fs = std::filesystem;

struct Symbol {
    std::string name;
    std::string type; // "class", "function", "struct", "method"
    std::string source; // "tree_sitter", "regex", etc.
    std::string path;
    int line;
    int endLine = 0; // 0 means unknown
    std::string signature;
};

class ISymbolProvider {
public:
    virtual ~ISymbolProvider() = default;
    virtual std::vector<Symbol> extractSymbols(const std::string& content, const std::string& relPath) const = 0;
    virtual bool supportsExtension(const std::string& ext) const = 0;
};

class SymbolManager {
public:
    using Symbol = ::Symbol;
    struct FileMeta {
        std::uintmax_t size = 0;
        std::time_t mtime = 0;
        std::uint64_t hash = 0;
    };

    SymbolManager(const std::string& rootPath);
    ~SymbolManager();

    void registerProvider(std::unique_ptr<ISymbolProvider> provider);
    void setFallbackOnEmpty(bool enabled) { fallbackOnEmpty = enabled; }

    // Start asynchronous full scan
    void startAsyncScan();

    // Start real-time file watching (incremental updates)
    void startWatching(int intervalSeconds = 5);

    // Stop watching
    void stopWatching();

    // Manually trigger update for a specific file
    void updateFile(const std::string& relPath);

    // Search for symbols matching a query
    std::vector<Symbol> search(const std::string& query);

    // Get all symbols in a specific file
    std::vector<Symbol> getFileSymbols(const std::string& relPath);

    bool isScanning() const { return scanning; }
    size_t getSymbolCount() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return symbols.size(); 
    }

private:
    std::string rootPath;
    std::vector<Symbol> symbols;
    std::vector<std::unique_ptr<ISymbolProvider>> providers;
    std::unordered_map<std::string, std::vector<Symbol>> fileSymbols;
    std::unordered_map<std::string, FileMeta> fileMeta;
    mutable std::mutex mtx;
    std::atomic<bool> scanning{false};
    std::thread scanThread;
    bool fallbackOnEmpty = false;

    // Real-time watching
    std::atomic<bool> watching{false};
    std::atomic<bool> stopWatch{false};
    std::thread watchThread;
    int watchInterval = 5;

    void performScan();
    void watchLoop();
    void checkFileChanges();
    void scanFile(const fs::path& filePath, std::vector<Symbol>& localSymbols);
    void updateSingleFile(const fs::path& filePath);
    fs::path getIndexPath() const;
    void loadIndex();
    void saveIndex();
    bool shouldIgnore(const fs::path& path);
};
