#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

class FileManager {
public:
    FileManager(const std::string& rootPath, const std::vector<std::string>& extensions = {});
    
    // 递归读取所有文件内容
    std::map<std::string, std::string> readAllFiles();
    
    // 搜索特定关键词的文件
    std::vector<std::string> searchFiles(const std::string& query);

private:
    std::string rootPath;
    std::vector<std::string> allowedExtensions;
    bool isTextFile(const fs::path& path);
};
