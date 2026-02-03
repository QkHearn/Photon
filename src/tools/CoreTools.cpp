#include "CoreTools.h"
#include <iostream>
#include <vector>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

// ============================================================================
// ReadCodeBlockTool Implementation
// ============================================================================

ReadCodeBlockTool::ReadCodeBlockTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string ReadCodeBlockTool::getDescription() const {
    return "Read a specific range of lines from a file. "
           "Parameters: file_path (string), start_line (int, optional), end_line (int, optional). "
           "If no line range specified, reads entire file.";
}

nlohmann::json ReadCodeBlockTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Relative path to the file"}
            }},
            {"start_line", {
                {"type", "integer"},
                {"description", "Starting line number (1-indexed, optional)"}
            }},
            {"end_line", {
                {"type", "integer"},
                {"description", "Ending line number (1-indexed, optional)"}
            }}
        }},
        {"required", {"file_path"}}
    };
}

nlohmann::json ReadCodeBlockTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("file_path")) {
        result["error"] = "Missing required parameter: file_path";
        return result;
    }
    
    std::string filePath = args["file_path"].get<std::string>();
    fs::path fullPath = rootPath / fs::u8path(filePath);
    
    // 检查文件是否存在
    if (!fs::exists(fullPath)) {
        result["error"] = "File not found: " + filePath;
        return result;
    }
    
    if (!fs::is_regular_file(fullPath)) {
        result["error"] = "Not a regular file: " + filePath;
        return result;
    }
    
    // 读取文件
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        result["error"] = "Failed to open file: " + filePath;
        return result;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    int totalLines = static_cast<int>(lines.size());
    int startLine = args.value("start_line", 1);
    int endLine = args.value("end_line", totalLines);
    
    // 边界检查
    if (startLine < 1) startLine = 1;
    if (endLine > totalLines) endLine = totalLines;
    if (startLine > endLine) {
        result["error"] = "Invalid range: start_line > end_line";
        return result;
    }
    
    // 构建内容
    std::ostringstream content;
    for (int i = startLine - 1; i < endLine; ++i) {
        content << (i + 1) << "|" << lines[i];
        if (i < endLine - 1) content << "\n";
    }
    
    // 返回结果
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = "File: " + filePath + "\n" +
                          "Lines: " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                          " (Total: " + std::to_string(totalLines) + ")\n\n" +
                          content.str();
    
    result["content"] = nlohmann::json::array({contentItem});
    return result;
}

// ============================================================================
// ApplyPatchTool Implementation
// ============================================================================

ApplyPatchTool::ApplyPatchTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string ApplyPatchTool::getDescription() const {
    return "Apply line-based edits to a file. "
           "Operations: 'insert' (add lines), 'replace' (replace lines), 'delete' (remove lines). "
           "Parameters: file_path, operation, start_line, end_line (for replace/delete), content (for insert/replace).";
}

nlohmann::json ApplyPatchTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"file_path", {
                {"type", "string"},
                {"description", "Relative path to the file"}
            }},
            {"operation", {
                {"type", "string"},
                {"enum", {"insert", "replace", "delete"}},
                {"description", "Edit operation type"}
            }},
            {"start_line", {
                {"type", "integer"},
                {"description", "Starting line number (1-indexed)"}
            }},
            {"end_line", {
                {"type", "integer"},
                {"description", "Ending line number (1-indexed, for replace/delete)"}
            }},
            {"content", {
                {"type", "string"},
                {"description", "Content to insert or replace (multi-line string)"}
            }}
        }},
        {"required", {"file_path", "operation", "start_line"}}
    };
}

void ApplyPatchTool::createBackup(const std::string& path) {
    fs::path backupDir = rootPath / ".photon" / "backups";
    fs::create_directories(backupDir);
    
    fs::path srcPath = rootPath / fs::u8path(path);
    fs::path dstPath = backupDir / fs::u8path(path);
    
    fs::create_directories(dstPath.parent_path());
    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);
}

nlohmann::json ApplyPatchTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("file_path") || !args.contains("operation") || !args.contains("start_line")) {
        result["error"] = "Missing required parameters";
        return result;
    }
    
    std::string filePath = args["file_path"].get<std::string>();
    std::string operation = args["operation"].get<std::string>();
    int startLine = args["start_line"].get<int>();
    
    fs::path fullPath = rootPath / fs::u8path(filePath);
    
    // 检查文件是否存在
    if (!fs::exists(fullPath)) {
        result["error"] = "File not found: " + filePath;
        return result;
    }
    
    // 创建备份
    try {
        createBackup(filePath);
    } catch (const std::exception& e) {
        result["error"] = std::string("Failed to create backup: ") + e.what();
        return result;
    }
    
    // 读取文件
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        result["error"] = "Failed to open file: " + filePath;
        return result;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    // 执行操作
    if (operation == "insert") {
        if (!args.contains("content")) {
            result["error"] = "Missing 'content' for insert operation";
            return result;
        }
        
        std::string content = args["content"].get<std::string>();
        std::istringstream iss(content);
        std::vector<std::string> newLines;
        while (std::getline(iss, line)) {
            newLines.push_back(line);
        }
        
        // 插入位置检查
        if (startLine < 1) startLine = 1;
        if (startLine > static_cast<int>(lines.size()) + 1) {
            startLine = static_cast<int>(lines.size()) + 1;
        }
        
        lines.insert(lines.begin() + startLine - 1, newLines.begin(), newLines.end());
        
    } else if (operation == "replace") {
        if (!args.contains("content") || !args.contains("end_line")) {
            result["error"] = "Missing 'content' or 'end_line' for replace operation";
            return result;
        }
        
        int endLine = args["end_line"].get<int>();
        std::string content = args["content"].get<std::string>();
        
        // 边界检查
        if (startLine < 1 || startLine > static_cast<int>(lines.size())) {
            result["error"] = "Invalid start_line";
            return result;
        }
        if (endLine < startLine || endLine > static_cast<int>(lines.size())) {
            result["error"] = "Invalid end_line";
            return result;
        }
        
        // 删除旧行
        lines.erase(lines.begin() + startLine - 1, lines.begin() + endLine);
        
        // 插入新行
        std::istringstream iss(content);
        std::vector<std::string> newLines;
        while (std::getline(iss, line)) {
            newLines.push_back(line);
        }
        lines.insert(lines.begin() + startLine - 1, newLines.begin(), newLines.end());
        
    } else if (operation == "delete") {
        int endLine = args.value("end_line", startLine);
        
        // 边界检查
        if (startLine < 1 || startLine > static_cast<int>(lines.size())) {
            result["error"] = "Invalid start_line";
            return result;
        }
        if (endLine < startLine || endLine > static_cast<int>(lines.size())) {
            result["error"] = "Invalid end_line";
            return result;
        }
        
        lines.erase(lines.begin() + startLine - 1, lines.begin() + endLine);
        
    } else {
        result["error"] = "Invalid operation: " + operation;
        return result;
    }
    
    // 写回文件
    std::ofstream outFile(fullPath);
    if (!outFile.is_open()) {
        result["error"] = "Failed to write file: " + filePath;
        return result;
    }
    
    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i < lines.size() - 1) outFile << "\n";
    }
    outFile.close();
    
    // 返回结果
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = "Successfully applied " + operation + " to " + filePath + 
                          " at lines " + std::to_string(startLine);
    
    result["content"] = nlohmann::json::array({contentItem});
    return result;
}

// ============================================================================
// RunCommandTool Implementation
// ============================================================================

RunCommandTool::RunCommandTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string RunCommandTool::getDescription() const {
    return "Execute a shell command in the project directory. "
           "Parameters: command (string), timeout (int, optional, default 30 seconds).";
}

nlohmann::json RunCommandTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Command to execute"}
            }},
            {"timeout", {
                {"type", "integer"},
                {"description", "Timeout in seconds (default 30)"}
            }}
        }},
        {"required", {"command"}}
    };
}

nlohmann::json RunCommandTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("command")) {
        result["error"] = "Missing required parameter: command";
        return result;
    }
    
    std::string command = args["command"].get<std::string>();
    
    // 切换到项目目录并执行命令
    std::string fullCommand = "cd \"" + rootPath.u8string() + "\" && " + command + " 2>&1";
    
    std::array<char, 128> buffer;
    std::string output;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(fullCommand.c_str(), "r"), pclose);
    if (!pipe) {
        result["error"] = "Failed to execute command";
        return result;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    
    int exitCode = pclose(pipe.release()) / 256;
    
    // 返回结果
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = "Command: " + command + "\n" +
                          "Exit Code: " + std::to_string(exitCode) + "\n\n" +
                          "Output:\n" + output;
    
    result["content"] = nlohmann::json::array({contentItem});
    result["exit_code"] = exitCode;
    return result;
}

// ============================================================================
// ListProjectFilesTool Implementation
// ============================================================================

ListProjectFilesTool::ListProjectFilesTool(const std::string& rootPath) 
    : rootPath(fs::u8path(rootPath)) {}

std::string ListProjectFilesTool::getDescription() const {
    return "List files and directories in the project. "
           "Parameters: path (string, optional, default '.'), max_depth (int, optional, default 3).";
}

nlohmann::json ListProjectFilesTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "Relative path to list (default '.')"}
            }},
            {"max_depth", {
                {"type", "integer"},
                {"description", "Maximum depth to recurse (default 3)"}
            }}
        }}
    };
}

void ListProjectFilesTool::listDirectory(const fs::path& dir, nlohmann::json& result, int maxDepth, int currentDepth) {
    if (currentDepth > maxDepth) return;
    
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::string name = entry.path().filename().u8string();
            
            // 跳过隐藏文件和常见忽略目录
            if (name[0] == '.' || name == "node_modules" || name == "build" || name == "dist") {
                continue;
            }
            
            nlohmann::json item;
            item["name"] = name;
            item["path"] = fs::relative(entry.path(), rootPath).u8string();
            item["type"] = entry.is_directory() ? "directory" : "file";
            
            if (entry.is_regular_file()) {
                item["size"] = entry.file_size();
            }
            
            if (entry.is_directory() && currentDepth < maxDepth) {
                nlohmann::json children = nlohmann::json::array();
                listDirectory(entry.path(), children, maxDepth, currentDepth + 1);
                item["children"] = children;
            }
            
            result.push_back(item);
        }
    } catch (const std::exception& e) {
        // 忽略权限错误等
    }
}

nlohmann::json ListProjectFilesTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    std::string path = args.value("path", ".");
    int maxDepth = args.value("max_depth", 3);
    
    fs::path fullPath = rootPath / fs::u8path(path);
    
    if (!fs::exists(fullPath)) {
        result["error"] = "Path not found: " + path;
        return result;
    }
    
    if (!fs::is_directory(fullPath)) {
        result["error"] = "Not a directory: " + path;
        return result;
    }
    
    nlohmann::json tree = nlohmann::json::array();
    listDirectory(fullPath, tree, maxDepth, 0);
    
    // 构建可读的树形结构文本
    std::ostringstream treeText;
    treeText << "Project Structure: " << path << "\n\n";
    
    std::function<void(const nlohmann::json&, int)> printTree;
    printTree = [&](const nlohmann::json& items, int depth) {
        for (const auto& item : items) {
            std::string indent(depth * 2, ' ');
            treeText << indent << "- " << item["name"].get<std::string>();
            
            if (item["type"] == "file" && item.contains("size")) {
                treeText << " (" << item["size"].get<size_t>() << " bytes)";
            }
            
            treeText << "\n";
            
            if (item.contains("children")) {
                printTree(item["children"], depth + 1);
            }
        }
    };
    
    printTree(tree, 0);
    
    nlohmann::json contentItem;
    contentItem["type"] = "text";
    contentItem["text"] = treeText.str();
    
    result["content"] = nlohmann::json::array({contentItem});
    result["tree"] = tree;
    return result;
}

// ============================================================================
// SkillActivateTool Implementation
// ============================================================================

#include "utils/SkillManager.h"

SkillActivateTool::SkillActivateTool(SkillManager* skillManager)
    : skillMgr(skillManager) {}

std::string SkillActivateTool::getDescription() const {
    return "Activate a specialized skill to access its capabilities. "
           "Once activated, the skill's tools and constraints will be injected "
           "into your context. Parameters: skill_name (string).";
}

nlohmann::json SkillActivateTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"skill_name", {
                {"type", "string"},
                {"description", "Name of the skill to activate"}
            }}
        }},
        {"required", {"skill_name"}}
    };
}

nlohmann::json SkillActivateTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("skill_name")) {
        result["error"] = "Missing required parameter: skill_name";
        return result;
    }
    
    std::string skillName = args["skill_name"].get<std::string>();
    
    if (!skillMgr) {
        result["error"] = "SkillManager not available";
        return result;
    }
    
    if (skillMgr->activate(skillName)) {
        result["success"] = true;
        result["message"] = "Skill activated: " + skillName;
        result["active_skills"] = skillMgr->getActiveSkills();
        
        // 返回 Skill 的 Prompt 片段
        result["skill_prompt"] = skillMgr->getActiveSkillsPrompt();
    } else {
        result["error"] = "Failed to activate skill: " + skillName;
        result["hint"] = "Check if skill exists in allowlist";
    }
    
    return result;
}

// ============================================================================
// SkillDeactivateTool Implementation
// ============================================================================

SkillDeactivateTool::SkillDeactivateTool(SkillManager* skillManager)
    : skillMgr(skillManager) {}

std::string SkillDeactivateTool::getDescription() const {
    return "Deactivate a previously activated skill to free up context space. "
           "Parameters: skill_name (string).";
}

nlohmann::json SkillDeactivateTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"skill_name", {
                {"type", "string"},
                {"description", "Name of the skill to deactivate"}
            }}
        }},
        {"required", {"skill_name"}}
    };
}

nlohmann::json SkillDeactivateTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    if (!args.contains("skill_name")) {
        result["error"] = "Missing required parameter: skill_name";
        return result;
    }
    
    std::string skillName = args["skill_name"].get<std::string>();
    
    if (!skillMgr) {
        result["error"] = "SkillManager not available";
        return result;
    }
    
    skillMgr->deactivate(skillName);
    
    result["success"] = true;
    result["message"] = "Skill deactivated: " + skillName;
    result["active_skills"] = skillMgr->getActiveSkills();
    
    return result;
}
