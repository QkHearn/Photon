#include "ScanIgnore.h"
#include <regex>
#include <algorithm>

struct ScanIgnoreRules::Impl {
    std::vector<std::string> patternStrings;
    mutable std::vector<std::regex> compiled;
    mutable bool compiledDone = false;

    void ensureCompiled() const {
        if (compiledDone) return;
        compiledDone = true;
        for (const auto& s : patternStrings) {
            try {
                compiled.emplace_back(s, std::regex::ECMAScript);
            } catch (...) {}
            // 无效正则则跳过该条
        }
    }
};

ScanIgnoreRules::ScanIgnoreRules(std::vector<std::string> patterns)
    : impl_(std::make_unique<Impl>()) {
    impl_->patternStrings = std::move(patterns);
    if (impl_->patternStrings.empty()) {
        impl_->patternStrings = {"node_modules", "build", "\\.venv", "dist"};
    }
}

ScanIgnoreRules::~ScanIgnoreRules() = default;

bool ScanIgnoreRules::shouldIgnore(const fs::path& path) const {
    // 1) 内置：任意路径段为「以 . 开头的目录」则忽略（不含 . / ..）
    for (const auto& comp : path) {
        std::string seg = comp.string();
        if (seg.empty()) continue;
        if (seg[0] == '.' && seg != "." && seg != "..") return true;
    }

    impl_->ensureCompiled();
    std::string p = path.generic_string();
    for (const auto& re : impl_->compiled) {
        try {
            if (std::regex_search(p, re)) return true;
        } catch (...) {}
    }
    return false;
}
