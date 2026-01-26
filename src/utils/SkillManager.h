#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

class SkillManager {
public:
    struct Skill {
        std::string name;
        std::string description;
        std::string content;
        std::string path;
    };

    // Sync skills from global roots to project .photon/skills and load them
    void syncAndLoad(const std::vector<std::string>& sourceRoots, const std::string& projectRootStr) {
        fs::path projectRoot = fs::u8path(projectRootStr);
        fs::path destRoot = projectRoot / ".photon" / "skills";

        if (!fs::exists(destRoot)) {
            try {
                fs::create_directories(destRoot);
            } catch (const std::exception& e) {
                std::cerr << "Error creating skill destination " << destRoot.string() << ": " << e.what() << std::endl;
                return;
            }
        }

        for (const auto& rootStr : sourceRoots) {
            fs::path rootPath = fs::u8path(rootStr);
            // Handle home directory expansion if needed
            if (rootStr.front() == '~') {
                const char* home = std::getenv("HOME");
                if (home) {
                    rootPath = fs::path(home) / rootStr.substr(2);
                }
            }

            if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) continue;

            try {
                // Recursively search for SKILL.md files in source
                for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
                    if (entry.is_regular_file() && entry.path().filename() == "SKILL.md") {
                        fs::path sourceSkillDir = entry.path().parent_path();
                        std::string skillName = sourceSkillDir.filename().string();
                        
                        fs::path destSkillDir = destRoot / skillName;
                        
                        // Create destination directory if it doesn't exist
                        if (!fs::exists(destSkillDir)) {
                            fs::create_directories(destSkillDir);
                        }

                        // Copy contents
                        // Note: fs::copy with recursive | overwrite_existing replaces files
                        fs::copy(sourceSkillDir, destSkillDir, fs::copy_options::overwrite_existing | fs::copy_options::recursive);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error syncing skills from " << rootStr << ": " << e.what() << std::endl;
            }
        }

        // Finally, load from the project-local .photon/skills directory
        loadFromRoot(destRoot.string());
    }

    // Load skills from a single root directory (each subdir is a skill)
    void loadFromRoot(const std::string& rootPathStr) {
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
                    loadSkill(skillName, entry.path().string());
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

    void loadSkill(const std::string& name, const std::string& path) {
        try {
            std::ifstream f(path);
            if (f.is_open()) {
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                Skill skill;
                skill.name = name;
                skill.path = path;
                skill.content = content;
                parseFrontmatter(skill);
                
                // If no description in frontmatter, use a generic one or first few lines? 
                // For now, leave empty if not found, or use "No description provided."
                if (skill.description.empty()) {
                    skill.description = "Extended capability for " + name;
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
