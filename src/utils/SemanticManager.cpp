#include "utils/SemanticManager.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>

SemanticManager::SemanticManager(const std::string& rootPath, std::shared_ptr<LLMClient> llmClient)
    : rootPath(rootPath), llmClient(llmClient) {
    loadIndex();
}

SemanticManager::~SemanticManager() {
    saveIndex();
}

void SemanticManager::addChunk(const SemanticChunk& chunk) {
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
    chunk.embedding = llmClient->getEmbedding(chunk.content);
    addChunk(chunk);
}

void SemanticManager::chunkMarkdown(const std::string& content, const std::string& relPath) {
    // Split by headers
    std::regex headerRegex(R"(^#{1,3}\s+(.*)$)", std::regex::multiline);
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
                SemanticChunk chunk;
                chunk.content = chunkText;
                chunk.path = relPath;
                chunk.type = "markdown";
                chunk.startLine = lineNum;
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
            SemanticChunk chunk;
            chunk.content = chunkText;
            chunk.path = relPath;
            chunk.type = "markdown";
            chunk.startLine = lineNum;
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
            SemanticChunk chunk;
            chunk.content = chunkText;
            chunk.path = relPath;
            chunk.type = "code";
            chunk.startLine = lineNum;
            chunk.embedding = llmClient->getEmbedding(chunk.content);
            addChunk(chunk);
        }
        
        lineNum += std::count(content.begin() + lastPos, content.begin() + currentPos, '\n');
        lastPos = currentPos + 2;
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

void SemanticManager::saveIndex() {
    nlohmann::json j = nlohmann::json::array();
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& chunk : chunks) {
        j.push_back({
            {"content", chunk.content},
            {"path", chunk.path},
            {"startLine", chunk.startLine},
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
            chunk.type = item.value("type", "");
            chunk.embedding = item.value("embedding", std::vector<float>());
            chunks.push_back(chunk);
        }
    } catch (...) {}
}
