#pragma once
#include "ITool.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// 前向声明
class SkillManager;

/**
 * @brief 读取代码块工具
 * 
 * 极简设计: 只读取指定文件的指定行范围
 * 不包含任何智能判断 (符号查找、上下文窗口等由 Agent 决定)
 */
class ReadCodeBlockTool : public ITool {
public:
    explicit ReadCodeBlockTool(const std::string& rootPath);
    
    std::string getName() const override { return "read_code_block"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
};

/**
 * @brief 应用补丁工具
 * 
 * 极简设计: 只支持行级编辑
 * - insert: 在指定行插入内容
 * - replace: 替换指定行范围
 * - delete: 删除指定行范围
 * 
 * 不支持全文件覆盖 (防止代码丢失)
 */
class ApplyPatchTool : public ITool {
public:
    explicit ApplyPatchTool(const std::string& rootPath);
    
    std::string getName() const override { return "apply_patch"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
    
    // 创建备份
    void createBackup(const std::string& path);
};

/**
 * @brief 运行命令工具
 * 
 * 极简设计: 只执行命令,不做任何过滤
 * 安全检查由 Agent 完成
 */
class RunCommandTool : public ITool {
public:
    explicit RunCommandTool(const std::string& rootPath);
    
    std::string getName() const override { return "run_command"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
};

/**
 * @brief 列出项目文件工具
 * 
 * 极简设计: 只列出目录结构
 * 不包含任何过滤逻辑 (由 Agent 决定读取哪些文件)
 */
class ListProjectFilesTool : public ITool {
public:
    explicit ListProjectFilesTool(const std::string& rootPath);
    
    std::string getName() const override { return "list_project_files"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
    
    // 递归列出目录
    void listDirectory(const fs::path& dir, nlohmann::json& result, int maxDepth, int currentDepth);
};

/**
 * @brief Skill 激活工具
 * 
 * 动态激活指定的 Skill,使其能力对 Agent 可用
 */
class SkillActivateTool : public ITool {
public:
    explicit SkillActivateTool(SkillManager* skillManager);
    
    std::string getName() const override { return "skill_activate"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    SkillManager* skillMgr;
};

/**
 * @brief Skill 停用工具
 * 
 * 停用指定的 Skill,释放其占用的上下文
 */
class SkillDeactivateTool : public ITool {
public:
    explicit SkillDeactivateTool(SkillManager* skillManager);
    
    std::string getName() const override { return "skill_deactivate"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    SkillManager* skillMgr;
};
