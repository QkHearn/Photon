#include "utils/SemanticManager.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <cstring>

#ifdef PHOTON_USE_SQLITE
#include <sqlite3.h>
#endif

SemanticManager::SemanticManager(const std::string& rootPath, std::shared_ptr<LLMClient> llmClient)
    : rootPath(rootPath), llmClient(llmClient) {
#ifdef PHOTON_USE_SQLITE
    useSqlite = initDb();
#endif
    loadIndex();
}

SemanticManager::~SemanticManager() {
    saveIndex();
#ifdef PHOTON_USE_SQLITE
    closeDb();
#endif
}

void SemanticManager::addChunk(const SemanticChunk& chunk) {
#ifdef PHOTON_USE_SQLITE
    if (useSqlite) {
        upsertChunkDb(chunk);
        return;
    }
#endif
    std::lock_guard<std::mutex> lock(mtx);
    // Update if exists, otherwise add
    auto it = std::find_if(chunks.begin(), chunks.end(), [&](const SemanticChunk& c) {
        return c.path == chunk.path && c.startLine == chunk.startLine && c.type == chunk.type;
    });
    
    if (it != chunks.end()) {
        *it = chunk;
    } else {
        chunks.push_back(chunk);
    }
}

float SemanticManager::cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size() || v1.empty()) return 0.0f;
    float dot = 0.0f, n1 = 0.0f, n2 = 0.0f;
    for (size_t i = 0; i < v1.size(); ++i) {
        dot += v1[i] * v2[i];
        n1 += v1[i] * v1[i];
        n2 += v2[i] * v2[i];
    }
    return dot / (std::sqrt(n1) * std::sqrt(n2));
}

std::vector<SemanticChunk> SemanticManager::search(const std::string& query, int topK) {
    auto queryEmbedding = llmClient->getEmbedding(query);
    if (queryEmbedding.empty()) return {};

#ifdef PHOTON_USE_SQLITE
    if (useSqlite) {
        return searchDb(queryEmbedding, topK);
    }
#endif
    std::vector<SemanticChunk> results;
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& chunk : chunks) {
            if (chunk.embedding.empty()) continue;
            chunk.score = cosineSimilarity(queryEmbedding, chunk.embedding);
            results.push_back(chunk);
        }
    }

    std::sort(results.begin(), results.end(), [](const SemanticChunk& a, const SemanticChunk& b) {
        return a.score > b.score;
    });

    if (results.size() > static_cast<size_t>(topK)) {
        results.resize(topK);
    }
    return results;
}

void SemanticManager::indexFile(const std::string& relPath, const std::string& type) {
    fs::path fullPath = fs::path(rootPath) / fs::u8path(relPath);
    if (!fs::exists(fullPath)) return;

    std::ifstream file(fullPath);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    removeChunksForFile(relPath, type);
    if (type == "markdown") {
        chunkMarkdown(content, relPath);
    } else if (type == "code") {
        chunkCode(content, relPath);
    }
}

void SemanticManager::indexFact(const std::string& key, const std::string& value) {
    SemanticChunk chunk;
    chunk.content = "Fact [" + key + "]: " + value;
    chunk.path = "memory.json";
    chunk.type = "fact";
    chunk.startLine = 0;
    chunk.endLine = 0;
    chunk.embedding = llmClient->getEmbedding(chunk.content);
    addChunk(chunk);
}

void SemanticManager::chunkMarkdown(const std::string& content, const std::string& relPath) {
    // Split by headers
    std::regex headerRegex(R"(^#{1,3}\s+(.*)$)");
    auto words_begin = std::sregex_iterator(content.begin(), content.end(), headerRegex);
    auto words_end = std::sregex_iterator();

    size_t lastPos = 0;
    int lineNum = 1;
    
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        size_t currentPos = match.position();
        
        if (currentPos > lastPos) {
            std::string chunkText = content.substr(lastPos, currentPos - lastPos);
            if (chunkText.length() > 50) {
                int chunkLineCount = 1 + static_cast<int>(std::count(chunkText.begin(), chunkText.end(), '\n'));
                SemanticChunk chunk;
                chunk.content = chunkText;
                chunk.path = relPath;
                chunk.type = "markdown";
                chunk.startLine = lineNum;
                chunk.endLine = lineNum + chunkLineCount - 1;
                chunk.embedding = llmClient->getEmbedding(chunk.content);
                addChunk(chunk);
            }
        }
        
        lastPos = currentPos;
        // Count lines to update lineNum
        lineNum += std::count(content.begin() + lastPos, content.begin() + currentPos, '\n');
    }
    
    // Last chunk
    if (lastPos < content.length()) {
        std::string chunkText = content.substr(lastPos);
        if (chunkText.length() > 50) {
            int chunkLineCount = 1 + static_cast<int>(std::count(chunkText.begin(), chunkText.end(), '\n'));
            SemanticChunk chunk;
            chunk.content = chunkText;
            chunk.path = relPath;
            chunk.type = "markdown";
            chunk.startLine = lineNum;
            chunk.endLine = lineNum + chunkLineCount - 1;
            chunk.embedding = llmClient->getEmbedding(chunk.content);
            addChunk(chunk);
        }
    }
}

void SemanticManager::chunkCode(const std::string& content, const std::string& relPath) {
    // Basic code chunking: split by large blocks or functions
    // For now, let's use a simple approach: split by double newlines if they contain logic
    std::regex blockRegex(R"(\n\n)");
    auto words_begin = std::sregex_iterator(content.begin(), content.end(), blockRegex);
    auto words_end = std::sregex_iterator();

    size_t lastPos = 0;
    int lineNum = 1;

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        size_t currentPos = match.position();
        
        if (currentPos - lastPos > 200) { // Only chunk if large enough
            std::string chunkText = content.substr(lastPos, currentPos - lastPos);
            int chunkLineCount = 1 + static_cast<int>(std::count(chunkText.begin(), chunkText.end(), '\n'));
            SemanticChunk chunk;
            chunk.content = chunkText;
            chunk.path = relPath;
            chunk.type = "code";
            chunk.startLine = lineNum;
            chunk.endLine = lineNum + chunkLineCount - 1;
            chunk.embedding = llmClient->getEmbedding(chunk.content);
            addChunk(chunk);
        }
        
        lineNum += std::count(content.begin() + lastPos, content.begin() + currentPos, '\n');
        lastPos = currentPos + 2;
    }

    if (lastPos < content.length()) {
        std::string chunkText = content.substr(lastPos);
        if (chunkText.length() > 200) {
            int chunkLineCount = 1 + static_cast<int>(std::count(chunkText.begin(), chunkText.end(), '\n'));
            SemanticChunk chunk;
            chunk.content = chunkText;
            chunk.path = relPath;
            chunk.type = "code";
            chunk.startLine = lineNum;
            chunk.endLine = lineNum + chunkLineCount - 1;
            chunk.embedding = llmClient->getEmbedding(chunk.content);
            addChunk(chunk);
        }
    }
}

void SemanticManager::startAsyncIndexing() {
    if (indexing) return;
    indexing = true;
    indexingThread = std::thread([this]() {
        try {
            // 1. Index Markdown files
            for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".md") {
                        std::string relPath = fs::relative(entry.path(), fs::path(rootPath)).generic_string();
                        indexFile(relPath, "markdown");
                    }
                }
            }

            // 2. Index Knowledge (memory.json)
            fs::path memoryPath = fs::path(rootPath) / ".photon" / "memory.json";
            if (fs::exists(memoryPath)) {
                std::ifstream f(memoryPath);
                nlohmann::json memory;
                if (f >> memory) {
                    if (memory.contains("facts")) {
                        for (auto& [key, val] : memory["facts"].items()) {
                            indexFact(key, val.get<std::string>());
                        }
                    }
                }
            }

            // 3. Index core code files (limited to avoid too many API calls)
            // In a real scenario, we'd be more selective or use a local model
            
            saveIndex();
        } catch (...) {}
        indexing = false;
    });
    indexingThread.detach();
}

fs::path SemanticManager::getIndexPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "semantic_index.json";
}

fs::path SemanticManager::getDbPath() const {
    return fs::path(rootPath) / ".photon" / "index" / "semantic_index.sqlite";
}

void SemanticManager::saveIndex() {
#ifdef PHOTON_USE_SQLITE
    if (useSqlite) return;
#endif
    nlohmann::json j = nlohmann::json::array();
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& chunk : chunks) {
        j.push_back({
            {"content", chunk.content},
            {"path", chunk.path},
            {"startLine", chunk.startLine},
            {"endLine", chunk.endLine},
            {"type", chunk.type},
            {"embedding", chunk.embedding}
        });
    }
    
    fs::path indexPath = getIndexPath();
    fs::create_directories(indexPath.parent_path());
    std::ofstream file(indexPath);
    if (file.is_open()) {
        file << j.dump();
    }
}

void SemanticManager::loadIndex() {
#ifdef PHOTON_USE_SQLITE
    if (useSqlite) return;
#endif
    fs::path indexPath = getIndexPath();
    if (!fs::exists(indexPath)) return;

    std::ifstream file(indexPath);
    if (!file.is_open()) return;

    try {
        nlohmann::json j;
        file >> j;
        std::lock_guard<std::mutex> lock(mtx);
        chunks.clear();
        for (const auto& item : j) {
            SemanticChunk chunk;
            chunk.content = item.value("content", "");
            chunk.path = item.value("path", "");
            chunk.startLine = item.value("startLine", 0);
            chunk.endLine = item.value("endLine", chunk.startLine);
            chunk.type = item.value("type", "");
            chunk.embedding = item.value("embedding", std::vector<float>());
            chunks.push_back(chunk);
        }
    } catch (...) {}
}

void SemanticManager::removeChunksForFile(const std::string& relPath, const std::string& type) {
#ifdef PHOTON_USE_SQLITE
    if (useSqlite) {
        removeChunksForFileDb(relPath, type);
        return;
    }
#endif
    std::lock_guard<std::mutex> lock(mtx);
    chunks.erase(std::remove_if(chunks.begin(), chunks.end(),
        [&](const SemanticChunk& c) { return c.path == relPath && c.type == type; }),
        chunks.end());
}

#ifdef PHOTON_USE_SQLITE
bool SemanticManager::initDb() {
    fs::path dbPath = getDbPath();
    fs::create_directories(dbPath.parent_path());
    if (sqlite3_open(dbPath.string().c_str(), &db) != SQLITE_OK) {
        closeDb();
        return false;
    }

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS semantic_chunks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT NOT NULL,"
        "start_line INTEGER NOT NULL,"
        "end_line INTEGER NOT NULL,"
        "type TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "embedding BLOB,"
        "embedding_dim INTEGER,"
        "UNIQUE(path, start_line, type)"
        ");";
    if (sqlite3_exec(db, createSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        closeDb();
        return false;
    }
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    return true;
}

void SemanticManager::closeDb() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

void SemanticManager::upsertChunkDb(const SemanticChunk& chunk) {
    if (!db) return;
    const char* sql =
        "INSERT OR REPLACE INTO semantic_chunks "
        "(path, start_line, end_line, type, content, embedding, embedding_dim) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, chunk.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, chunk.startLine);
    sqlite3_bind_int(stmt, 3, chunk.endLine);
    sqlite3_bind_text(stmt, 4, chunk.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, chunk.content.c_str(), -1, SQLITE_TRANSIENT);
    if (!chunk.embedding.empty()) {
        const char* blob = reinterpret_cast<const char*>(chunk.embedding.data());
        int byteSize = static_cast<int>(chunk.embedding.size() * sizeof(float));
        sqlite3_bind_blob(stmt, 6, blob, byteSize, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 7, static_cast<int>(chunk.embedding.size()));
    } else {
        sqlite3_bind_null(stmt, 6);
        sqlite3_bind_null(stmt, 7);
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SemanticManager::removeChunksForFileDb(const std::string& relPath, const std::string& type) {
    if (!db) return;
    const char* sql = "DELETE FROM semantic_chunks WHERE path = ? AND type = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, relPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<SemanticChunk> SemanticManager::searchDb(const std::vector<float>& queryEmbedding, int topK) {
    std::vector<SemanticChunk> results;
    if (!db || queryEmbedding.empty()) return results;

    const char* sql = "SELECT content, path, start_line, end_line, type, embedding, embedding_dim FROM semantic_chunks;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* content = sqlite3_column_text(stmt, 0);
        const unsigned char* path = sqlite3_column_text(stmt, 1);
        int startLine = sqlite3_column_int(stmt, 2);
        int endLine = sqlite3_column_int(stmt, 3);
        const unsigned char* type = sqlite3_column_text(stmt, 4);
        const void* blob = sqlite3_column_blob(stmt, 5);
        int blobSize = sqlite3_column_bytes(stmt, 5);
        int dim = sqlite3_column_int(stmt, 6);

        if (!blob || blobSize <= 0 || dim <= 0) continue;
        if (static_cast<size_t>(dim) != queryEmbedding.size()) continue;

        std::vector<float> embedding(dim);
        std::memcpy(embedding.data(), blob, static_cast<size_t>(blobSize));

        SemanticChunk chunk;
        chunk.content = content ? reinterpret_cast<const char*>(content) : "";
        chunk.path = path ? reinterpret_cast<const char*>(path) : "";
        chunk.type = type ? reinterpret_cast<const char*>(type) : "";
        chunk.startLine = startLine;
        chunk.endLine = endLine;
        chunk.embedding = std::move(embedding);
        chunk.score = cosineSimilarity(queryEmbedding, chunk.embedding);
        results.push_back(std::move(chunk));
    }

    sqlite3_finalize(stmt);

    std::sort(results.begin(), results.end(), [](const SemanticChunk& a, const SemanticChunk& b) {
        return a.score > b.score;
    });
    if (results.size() > static_cast<size_t>(topK)) {
        results.resize(topK);
    }
    return results;
}
#endif
