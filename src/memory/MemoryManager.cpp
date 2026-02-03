#include "MemoryManager.h"
#include "ProjectMemory.h"
#include "FailureMemory.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// 简单的用户偏好实现
class UserPreference {
public:
    explicit UserPreference(const std::string& rootPath) {
        fs::path memoryDir = fs::u8path(rootPath) / ".photon" / "memory";
        fs::create_directories(memoryDir);
        memoryFile = memoryDir / "preferences.json";
    }
    
    void load() {
        if (!fs::exists(memoryFile)) return;
        
        try {
            std::ifstream file(memoryFile);
            if (file.is_open()) {
                file >> data;
                file.close();
            }
        } catch (...) {}
    }
    
    void save() {
        try {
            std::ofstream file(memoryFile);
            if (file.is_open()) {
                file << data.dump(2);
                file.close();
            }
        } catch (...) {}
    }
    
    std::string get(const std::string& key, const std::string& defaultValue) {
        if (data.contains(key)) {
            return data[key].get<std::string>();
        }
        return defaultValue;
    }
    
    void set(const std::string& key, const std::string& value) {
        data[key] = value;
        save();
    }

private:
    fs::path memoryFile;
    nlohmann::json data = nlohmann::json::object();
};

MemoryManager::MemoryManager(const std::string& rootPath) 
    : rootPath(rootPath) {
    
    fs::path memoryDir = fs::u8path(rootPath) / ".photon" / "memory";
    memoryPath = memoryDir.u8string();
    fs::create_directories(memoryDir);
    
    projectMemory = std::make_unique<ProjectMemory>(rootPath);
    failureMemory = std::make_unique<FailureMemory>(rootPath);
    userPreference = std::make_unique<UserPreference>(rootPath);
}

MemoryManager::~MemoryManager() {
    // 自动保存
    save();
}

void MemoryManager::load() {
    projectMemory->load();
    failureMemory->load();
    userPreference->load();
}

void MemoryManager::save() {
    projectMemory->save();
    failureMemory->save();
    userPreference->save();
}

// ProjectMemory 接口实现
std::string MemoryManager::getProjectType() {
    return projectMemory->getProjectType();
}

std::string MemoryManager::getBuildSystem() {
    return projectMemory->getBuildSystem();
}

std::vector<std::string> MemoryManager::getToolchain() {
    return projectMemory->getToolchain();
}

void MemoryManager::setArchitectureNote(const std::string& note) {
    projectMemory->setArchitectureNote(note);
    save();
}

void MemoryManager::addCodingConvention(const std::string& rule) {
    projectMemory->addCodingConvention(rule);
    save();
}

// FailureMemory 接口实现
void MemoryManager::recordFailure(const std::string& tool, 
                                 const nlohmann::json& args,
                                 const std::string& error) {
    failureMemory->recordFailure(tool, args, error);
}

void MemoryManager::recordSolution(const std::string& error, 
                                  const std::string& solution) {
    failureMemory->recordSolution(error, solution);
}

bool MemoryManager::hasSimilarFailure(const std::string& error) {
    return failureMemory->hasSimilarFailure(error);
}

std::string MemoryManager::getSolution(const std::string& error) {
    return failureMemory->getSolution(error);
}

// UserPreference 接口实现
std::string MemoryManager::getPreference(const std::string& key, const std::string& defaultValue) {
    return userPreference->get(key, defaultValue);
}

void MemoryManager::setPreference(const std::string& key, const std::string& value) {
    userPreference->set(key, value);
}
