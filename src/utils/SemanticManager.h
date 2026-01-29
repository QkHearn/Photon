#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <filesystem>
#include "core/LLMClient.h"

namespace fs = std::filesystem;

#ifdef PHOTON_USE_SQLITE
struct sqlite3;
#endif

struct SemanticChunk {
    std::string id;
    std::string content;
    std::string path;
    int startLine;
    int endLine;
    std::string type; // "code", "markdown", "fact", "skill"
    std::vector<float> embedding;
    float score = 0.0f; // For search results
};

class SemanticManager {
public:
    SemanticManager(const std::string& rootPath, std::shared_ptr<LLMClient> llmClient);
    ~SemanticManager();

    // Add or update a chunk in the index
    void addChunk(const SemanticChunk& chunk);
    
    // Rebuild index for a specific file
    void indexFile(const std::string& relPath, const std::string& type);
    void indexFact(const std::string& key, const std::string& value);
    
    // Unify search across code and knowledge
    std::vector<SemanticChunk> search(const std::string& query, int topK = 5);

    // Save/Load index
    void saveIndex();
    void loadIndex();

    // Background indexing
    void startAsyncIndexing();

private:
    std::string rootPath;
    std::shared_ptr<LLMClient> llmClient;
    std::vector<SemanticChunk> chunks;
    std::thread indexingThread;
    std::atomic<bool> indexing{false};
    mutable std::mutex mtx;
    bool useSqlite = false;
#ifdef PHOTON_USE_SQLITE
    sqlite3* db = nullptr;
    bool initDb();
    void closeDb();
    void upsertChunkDb(const SemanticChunk& chunk);
    void removeChunksForFileDb(const std::string& relPath, const std::string& type);
    std::vector<SemanticChunk> searchDb(const std::vector<float>& queryEmbedding, int topK);
#endif
    
    fs::path getIndexPath() const;
    fs::path getDbPath() const;
    float cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2);
    
    // Chunking helpers
    void chunkMarkdown(const std::string& content, const std::string& relPath);
    void chunkCode(const std::string& content, const std::string& relPath);
    void removeChunksForFile(const std::string& relPath, const std::string& type);
};
