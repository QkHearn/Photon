#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * 扫描忽略规则：符号扫描、call graph、list_project_files 等共用的「是否跳过路径」逻辑。
 * - 内置：以 . 开头的目录（除 . / ..）一律忽略。
 * - 配置：symbol_ignore_patterns 为正则，路径与之匹配则忽略。
 * 使用同一 ScanIgnoreRules 实例保证行为一致。
 */
class ScanIgnoreRules {
public:
    /** patterns 为正则表达式（ECMAScript），如 "build", "\\.git", "third_party"（字面点需写 \\.） */
    explicit ScanIgnoreRules(std::vector<std::string> patterns);
    ~ScanIgnoreRules();

    bool shouldIgnore(const fs::path& path) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
