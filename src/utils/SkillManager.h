#pragma once
#include <string>
#include <vector>
#include <map>
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
        bool isBuiltin = false; // 新增：标记是否为内置 Skill
    };

    SkillManager() {
        loadInternalStaticSkills();
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

    std::string getSystemPromptAddition() const {
        if (skills.empty()) return "";
        
        std::string prompt = "\n\n# Specialized Skills (Lazy Loading)\n";
        prompt += "You have access to the following specialized skills. "
                  "To use a skill, you MUST first read its full guide using the `skill_read` tool.\n"
                  "DO NOT guess the skill usage. Always read it first when relevant.\n\n";
        
        for (const auto& [name, skill] : skills) {
            prompt += "- **" + name + "**: " + skill.description + "\n";
        }
        return prompt;
    }

    std::string getSkillContent(const std::string& name) {
        if (skills.count(name)) {
            return skills[name].content;
        }
        return "Skill not found: " + name;
    }

    size_t getCount() const { return skills.size(); }
    
    const std::map<std::string, Skill>& getSkills() const { return skills; }

private:
    std::map<std::string, Skill> skills;
};
