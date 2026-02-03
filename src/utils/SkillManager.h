#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "utils/BuiltinSkillsData.h"

namespace fs = std::filesystem;

class SkillManager {
public:
    struct Skill {
        std::string name;
        std::string description;
        std::string content;
        std::string path;
        bool isBuiltin = false;
        
        // Skill 元数据 (从 SKILL.md 解析)
        std::vector<std::string> requiredTools;    // 需要的工具列表
        std::vector<std::string> constraints;       // 约束条件
        std::string minimalInterface;              // 最小接口描述
    };

    SkillManager() {
        loadInternalStaticSkills();
    }
    
    // ========== 动态激活 API ==========
    
    /**
     * @brief 激活指定 Skill
     * @param name Skill 名称
     * @return 是否成功激活
     * 
     * 激活时:
     * 1. 检查 Skill 是否在 allowlist 中
     * 2. 检查 Skill 所需工具是否可用
     * 3. 标记为激活状态
     * 4. 返回成功/失败
     */
    bool activate(const std::string& name) {
        if (skills.find(name) == skills.end()) {
            std::cerr << "[SkillManager] Skill not found: " << name << std::endl;
            return false;
        }
        
        // TODO: 检查工具可用性
        
        activeSkills.insert(name);
        std::cout << "[SkillManager] Activated skill: " << name << std::endl;
        return true;
    }
    
    /**
     * @brief 停用指定 Skill
     * @param name Skill 名称
     */
    void deactivate(const std::string& name) {
        activeSkills.erase(name);
        std::cout << "[SkillManager] Deactivated skill: " << name << std::endl;
    }
    
    /**
     * @brief 停用所有 Skill
     */
    void deactivateAll() {
        activeSkills.clear();
        std::cout << "[SkillManager] Deactivated all skills" << std::endl;
    }
    
    /**
     * @brief 检查 Skill 是否已激活
     */
    bool isActive(const std::string& name) const {
        return activeSkills.find(name) != activeSkills.end();
    }
    
    /**
     * @brief 获取当前激活的 Skill 列表
     */
    std::vector<std::string> getActiveSkills() const {
        return std::vector<std::string>(activeSkills.begin(), activeSkills.end());
    }

    void loadInternalStaticSkills() {
        for (const auto& [id, data] : INTERNAL_SKILLS_DATA) {
            Skill s;
            s.name = data.name;
            s.description = data.description;
            s.content = data.content;
            s.path = "embedded://builtin/" + id;
            s.isBuiltin = true;
            skills[id] = s;
        }
    }

    // Sync skills from global roots to global .photon/skills and load them
    void syncAndLoad(const std::vector<std::string>& sourceRoots, const std::string& globalDataPathStr) {
        fs::path globalDataPath = fs::u8path(globalDataPathStr);
        fs::path destRoot = globalDataPath / "skills";

        if (!fs::exists(destRoot)) {
            try {
                fs::create_directories(destRoot);
            } catch (const std::exception& e) {
                std::cerr << "Error creating skill destination " << destRoot.string() << ": " << e.what() << std::endl;
                return;
            }
        }

        bool anyChanged = false;
        for (const auto& rootStr : sourceRoots) {
            fs::path rootPath = fs::u8path(rootStr);
            if (rootStr.front() == '~') {
                const char* home = nullptr;
#ifdef _WIN32
                home = std::getenv("USERPROFILE");
#else
                home = std::getenv("HOME");
#endif
                if (home) rootPath = fs::path(home) / rootStr.substr(2);
            }

            if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) continue;

            try {
                for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
                    if (entry.is_regular_file() && entry.path().filename() == "SKILL.md") {
                        fs::path sourceSkillDir = entry.path().parent_path();
                        std::string skillName = sourceSkillDir.filename().string();
                        fs::path destSkillDir = destRoot / skillName;
                        
                        // 增量检查：只有当源文件更新时才拷贝
                        bool needsCopy = true;
                        if (fs::exists(destSkillDir / "SKILL.md")) {
                            if (fs::last_write_time(entry.path()) <= fs::last_write_time(destSkillDir / "SKILL.md")) {
                                needsCopy = false;
                            }
                        }

                        if (needsCopy) {
                            if (!fs::exists(destSkillDir)) fs::create_directories(destSkillDir);
                            fs::copy(sourceSkillDir, destSkillDir, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
                            anyChanged = true;
                        }
                    }
                }
            } catch (...) {}
        }

        // 加载最终结果
        loadFromRoot(destRoot.string());
    }

    // Load skills from a single root directory (each subdir is a skill)
    void loadFromRoot(const std::string& rootPathStr, bool isBuiltin = false) {
        try {
            fs::path rootPath = fs::u8path(rootPathStr);
            // Handle home directory expansion if needed (simplified)
            if (rootPathStr.front() == '~') {
                const char* home = std::getenv("HOME");
                if (home) {
                    rootPath = fs::path(home) / rootPathStr.substr(2);
                }
            }

            if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) return;

            // Recursively search for SKILL.md files
            for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
                if (entry.is_regular_file() && entry.path().filename() == "SKILL.md") {
                    // Use parent directory name as skill name
                    std::string skillName = entry.path().parent_path().filename().string();
                    loadSkill(skillName, entry.path().string(), isBuiltin);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning skill root " << rootPathStr << ": " << e.what() << std::endl;
        }
    }

    // Helper to parse YAML frontmatter manually
    void parseFrontmatter(Skill& skill) {
        if (skill.content.substr(0, 3) != "---") return;
        
        size_t endPos = skill.content.find("---", 3);
        if (endPos == std::string::npos) return;

        std::string yaml = skill.content.substr(3, endPos - 3);
        std::stringstream ss(yaml);
        std::string line;
        
        while (std::getline(ss, line)) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                val.erase(0, val.find_first_not_of(" \t"));
                val.erase(val.find_last_not_of(" \t") + 1);

                if (key == "description") skill.description = val;
                // name is usually already derived from folder, but frontmatter can override
                if (key == "name") skill.name = val;
            }
        }
    }

    void loadSkill(const std::string& name, const std::string& path, bool isBuiltin = false) {
        try {
            std::ifstream f(path);
            if (f.is_open()) {
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                Skill skill;
                skill.name = name;
                skill.path = path;
                skill.content = content;
                skill.isBuiltin = isBuiltin;
                parseFrontmatter(skill);
                
                if (skill.description.empty()) {
                    skill.description = "Extended capability for " + name;
                }

                // If we are overwriting an existing skill, and the new one is NOT builtin 
                // but the old one WAS, we check if it's actually the same skill.
                if (skills.count(name) && !isBuiltin && skills[name].isBuiltin) {
                    // If we are loading from .photon/skills, it might be a synced version of a builtin.
                    // Let's keep it marked as builtin if the name matches.
                    skill.isBuiltin = true;
                }

                skills[name] = skill;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error loading skill " << name << ": " << e.what() << std::endl;
        }
    }

    /**
     * @brief 获取 Skill 发现 Prompt (仅列出可用 Skill,不注入内容)
     * 
     * 这个 Prompt 在初始化时注入,告诉 LLM 有哪些 Skill 可用
     * 但不会注入 Skill 的具体内容
     */
    std::string getSkillDiscoveryPrompt() const {
        if (skills.empty()) return "";
        
        std::string prompt = "\n\n# Available Skills (Lazy Loading Required)\n";
        prompt += "You have access to specialized skills. Each skill must be explicitly activated before use.\n";
        prompt += "To activate a skill: call `skill_activate(name)` first, then use its capabilities.\n\n";
        prompt += "Available skills:\n";
        
        for (const auto& [name, skill] : skills) {
            prompt += "- **" + name + "**: " + skill.description + "\n";
        }
        
        prompt += "\n⚠️  IMPORTANT: Skills are NOT active by default. You MUST activate them when needed.\n";
        return prompt;
    }
    
    /**
     * @brief 获取激活 Skill 的 Prompt 片段 (动态注入)
     * 
     * 这个 Prompt 只在 Skill 被激活时才注入到 LLM 上下文
     * 包含 Skill 的工具、约束和接口
     */
    std::string getActiveSkillsPrompt() const {
        if (activeSkills.empty()) return "";
        
        std::string prompt = "\n\n# ACTIVATED SKILLS\n\n";
        
        for (const auto& name : activeSkills) {
            auto it = skills.find(name);
            if (it == skills.end()) continue;
            
            const Skill& skill = it->second;
            
            prompt += "## Skill: " + skill.name + "\n";
            prompt += "**Description**: " + skill.description + "\n\n";
            
            if (!skill.requiredTools.empty()) {
                prompt += "**Allowed Tools**:\n";
                for (const auto& tool : skill.requiredTools) {
                    prompt += "  - " + tool + "\n";
                }
                prompt += "\n";
            }
            
            if (!skill.constraints.empty()) {
                prompt += "**Constraints**:\n";
                for (const auto& constraint : skill.constraints) {
                    prompt += "  - " + constraint + "\n";
                }
                prompt += "\n";
            }
            
            if (!skill.minimalInterface.empty()) {
                prompt += "**Interface**:\n" + skill.minimalInterface + "\n\n";
            }
            
            prompt += "---\n\n";
        }
        
        return prompt;
    }
    
    /**
     * @brief 获取完整 Skill 内容 (用于 skill_read 工具)
     */
    std::string getSkillContent(const std::string& name) {
        if (skills.count(name)) {
            return skills[name].content;
        }
        return "Skill not found: " + name;
    }
    
    /**
     * @brief 旧接口 - 保持兼容性
     * @deprecated 使用 getSkillDiscoveryPrompt() 代替
     */
    std::string getSystemPromptAddition() const {
        return getSkillDiscoveryPrompt();
    }

    size_t getCount() const { return skills.size(); }
    
    const std::map<std::string, Skill>& getSkills() const { return skills; }

private:
    std::map<std::string, Skill> skills;      // 所有已加载的 Skill (Allowlist)
    std::set<std::string> activeSkills;       // 当前激活的 Skill
};
