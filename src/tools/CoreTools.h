#pragma once
#include "ITool.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// 前向声明
class SkillManager;
class SymbolManager;
struct Symbol;

// UTF-8 验证和清理辅助函数
namespace UTF8Utils {
    /**
     * @brief 验证并清理 UTF-8 字符串，替换无效字节为 '?'
     */
    std::string sanitize(const std::string& input);
}

/**
 * @brief 读取代码块工具
 * 
 * 智能设计: 根据参数自动选择最佳读取策略
 * - 无参数 + 代码文件 → 返回符号摘要
 * - 指定 symbol_name → 返回符号代码
 * - 指定 start_line/end_line → 返回指定行
 * - 其他情况 → 返回全文
 */
class ReadCodeBlockTool : public ITool {
public:
    explicit ReadCodeBlockTool(const std::string& rootPath, SymbolManager* symbolMgr = nullptr, bool enableDebug = false);
    
    std::string getName() const override { return "read_code_block"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
    SymbolManager* symbolMgr;
    bool enableDebug;
    
    // 检查是否是代码文件
    bool isCodeFile(const std::string& filePath) const;
    
    // 生成符号摘要
    nlohmann::json generateSymbolSummary(const std::string& filePath);
    
    // 生成符号摘要（非阻塞版本）
    nlohmann::json generateSymbolSummaryNonBlocking(const std::string& filePath);
    
    // 读取指定符号的代码
    nlohmann::json readSymbolCode(const std::string& filePath, const std::string& symbolName);
    
    // 读取指定行范围
    nlohmann::json readLineRange(const std::string& filePath, int startLine, int endLine);
    
    // 读取全文
    nlohmann::json readFullFile(const std::string& filePath);
    
    // 临时提取符号(用于项目外文件)
    std::vector<Symbol> extractSymbolsOnDemand(const fs::path& filePath);
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
    explicit ApplyPatchTool(const std::string& rootPath, bool hasGit = false);
    
    std::string getName() const override { return "apply_patch"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    fs::path rootPath;
    bool hasGit;
    
    // 创建备份 - 优先使用Git
    void createBackup(const std::string& path);
    
    // Git备份
    bool createGitBackup(const std::string& path);
    
    // 本地备份（回退方案）
    void createLocalBackup(const std::string& path);
    
    // 使用Git写入文件
    bool writeFileWithGit(const std::string& path, const std::vector<std::string>& lines);
    
    // Git风格diff支持（核心功能）。可选传入 applyError 以获取失败时的详细原因（如文件路径、上下文不匹配）。
    bool applyUnifiedDiff(const std::string& diffContent, std::string* applyError = nullptr);
    std::string generateUnifiedDiff(const std::vector<std::string>& files);
    bool applyFileDiff(const std::string& filePath, const std::string& diffContent);
    
    // Diff解析和应用
    struct DiffHunk {
        int oldStart, oldCount, newStart, newCount;
        std::vector<std::string> lines; // +, -, 空格前缀
    };
    
    struct FileDiff {
        std::string oldFile;
        std::string newFile;
        bool isNewFile;
        bool isDeleted;
        std::vector<DiffHunk> hunks;
    };
    
    std::vector<FileDiff> parseUnifiedDiff(const std::string& diffContent);
    // 若 applyError 非空且应用失败，会写入具体原因（如：文件路径、行号、上下文不匹配）。
    bool applyFileChanges(const FileDiff& fileDiff, std::string* applyError = nullptr);
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
 * @brief 代码搜索工具（grep）
 *
 * 在项目目录下按文本/正则查找，返回 文件:行号:内容。便于「不知道在哪个文件」时先定位再 read_code_block。
 */
class GrepTool : public ITool {
public:
    explicit GrepTool(const std::string& rootPath);
    std::string getName() const override { return "grep"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;
private:
    fs::path rootPath;
};

/**
 * @brief 用户 Attempt 工具：持久化当前任务意图与状态，避免多轮遗忘
 *
 * 存于 .photon/current_attempt.json。模型每轮可 get 回忆「正在做什么」，
 * 按 intent/read_scope 推进，用 update 记录进度或完成，用 clear 清空以便新需求。
 */
class AttemptTool : public ITool {
public:
    explicit AttemptTool(const std::string& rootPath);
    std::string getName() const override { return "attempt"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;
private:
    fs::path rootPath;
};

/**
 * @brief 语法/构建检查工具（少 token）
 *
 * 在项目目录执行构建（如 cmake --build build），仅返回前 N 行输出或仅错误行，
 * 便于用较少 token 发现编译/语法问题。
 */
class SyntaxCheckTool : public ITool {
public:
    explicit SyntaxCheckTool(const std::string& rootPath);
    std::string getName() const override { return "syntax_check"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;
private:
    fs::path rootPath;
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
