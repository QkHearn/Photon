#include "FailureMemory.h"
#include <fstream>
#include <algorithm>

nlohmann::json FailureMemory::Failure::toJson() const {
    nlohmann::json j;
    j["tool"] = toolName;
    j["args"] = args;
    j["error"] = error;
    j["solution"] = solution;
    j["timestamp"] = static_cast<long long>(timestamp);
    return j;
}

FailureMemory::Failure FailureMemory::Failure::fromJson(const nlohmann::json& j) {
    Failure f;
    f.toolName = j.value("tool", "");
    f.args = j.value("args", nlohmann::json::object());
    f.error = j.value("error", "");
    f.solution = j.value("solution", "");
    f.timestamp = static_cast<std::time_t>(j.value("timestamp", 0LL));
    return f;
}

FailureMemory::FailureMemory(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {
    
    fs::path memoryDir = this->rootPath / ".photon" / "memory";
    fs::create_directories(memoryDir);
    
    memoryFile = memoryDir / "failures.json";
}

void FailureMemory::load() {
    if (!fs::exists(memoryFile)) {
        return;
    }
    
    try {
        std::ifstream file(memoryFile);
        if (file.is_open()) {
            nlohmann::json data;
            file >> data;
            file.close();
            
            failures.clear();
            if (data.is_array()) {
                for (const auto& item : data) {
                    failures.push_back(Failure::fromJson(item));
                }
            }
        }
    } catch (...) {
        // 加载失败,保持空列表
    }
}

void FailureMemory::save() {
    try {
        nlohmann::json data = nlohmann::json::array();
        
        // 只保留最近的 maxFailures 条记录
        size_t start = failures.size() > maxFailures ? failures.size() - maxFailures : 0;
        for (size_t i = start; i < failures.size(); ++i) {
            data.push_back(failures[i].toJson());
        }
        
        std::ofstream file(memoryFile);
        if (file.is_open()) {
            file << data.dump(2);
            file.close();
        }
    } catch (...) {
        // 保存失败,忽略
    }
}

void FailureMemory::recordFailure(const std::string& tool, 
                                  const nlohmann::json& args,
                                  const std::string& error) {
    Failure f;
    f.toolName = tool;
    f.args = args;
    f.error = error;
    f.solution = "";
    f.timestamp = std::time(nullptr);
    
    failures.push_back(f);
    
    // 自动保存
    save();
}

void FailureMemory::recordSolution(const std::string& error, 
                                   const std::string& solution) {
    // 查找最近的匹配失败
    for (auto it = failures.rbegin(); it != failures.rend(); ++it) {
        if (calculateSimilarity(it->error, error) > 0.8) {
            it->solution = solution;
            save();
            return;
        }
    }
}

bool FailureMemory::hasSimilarFailure(const std::string& error) {
    for (const auto& f : failures) {
        if (calculateSimilarity(f.error, error) > 0.8 && !f.solution.empty()) {
            return true;
        }
    }
    return false;
}

std::string FailureMemory::getSolution(const std::string& error) {
    double maxSim = 0.0;
    std::string bestSolution;
    
    for (const auto& f : failures) {
        double sim = calculateSimilarity(f.error, error);
        if (sim > maxSim && sim > 0.8 && !f.solution.empty()) {
            maxSim = sim;
            bestSolution = f.solution;
        }
    }
    
    return bestSolution;
}

double FailureMemory::calculateSimilarity(const std::string& error1, const std::string& error2) {
    // 简单的相似度计算: 检查关键词是否匹配
    if (error1 == error2) return 1.0;
    
    // 转换为小写
    std::string e1 = error1, e2 = error2;
    std::transform(e1.begin(), e1.end(), e1.begin(), ::tolower);
    std::transform(e2.begin(), e2.end(), e2.begin(), ::tolower);
    
    // 检查是否包含
    if (e1.find(e2) != std::string::npos || e2.find(e1) != std::string::npos) {
        return 0.9;
    }
    
    // TODO: 实现更复杂的相似度算法 (Levenshtein 距离等)
    
    return 0.0;
}
