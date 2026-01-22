#include "FileManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

FileManager::FileManager(const std::string& rootPath, const std::vector<std::string>& extensions) 
    : rootPath(rootPath), allowedExtensions(extensions) {
    if (allowedExtensions.empty()) {
        allowedExtensions = {".txt", ".cpp", ".h", ".hpp", ".c", ".py", ".md", ".json", ".cmake", ".yml", ".yaml"};
    }
}

bool FileManager::isTextFile(const fs::path& path) {
    if (!path.has_extension()) return false;
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(allowedExtensions.begin(), allowedExtensions.end(), ext) != allowedExtensions.end();
}

std::map<std::string, std::string> FileManager::readAllFiles() {
    std::map<std::string, std::string> filesData;
    try {
        if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
            std::cerr << "Error: Path does not exist or is not a directory: " << rootPath << std::endl;
            return filesData;
        }

        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file() && isTextFile(entry.path())) {
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    filesData[entry.path().string()] = content;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    return filesData;
}

std::vector<std::string> FileManager::searchFiles(const std::string& query) {
    std::vector<std::string> matches;
    auto allFiles = readAllFiles();
    for (const auto& [path, content] : allFiles) {
        if (content.find(query) != std::string::npos || path.find(query) != std::string::npos) {
            matches.push_back(path);
        }
    }
    return matches;
}
