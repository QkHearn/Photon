#pragma once
#include <map>
#include <string>

struct BuiltinSkillData {
    std::string name;
    std::string description;
    std::string content;
};

static const std::map<std::string, BuiltinSkillData> INTERNAL_SKILLS_DATA = {
    {"code-architect", {
        "code-architect",
        "Photon Native Architect: Expert at project restructuring, directory organization, and maintaining build system integrity.",
        "# Skill: Photon Architect\nYou are an expert in C++ project structure and engineering best practices within the Photon ecosystem. Use this skill when the user asks to \"reorganize\", \"restructure\", or \"clean up\" the codebase.\n\n## Thinking Process\n1. **Map the Land**: Use `list_dir_tree` (depth 3) to understand the current hierarchy.\n2. **Understand the Build**: Read `CMakeLists.txt` or build scripts to see how files are linked.\n3. **Plan Safely**:\n   - Group files by responsibility (e.g., `core`, `mcp`, `utils`, `api`).\n   - Identify header-to-header dependencies.\n4. **Execute Methodically**:\n   - Move files using `bash_execute` (mv).\n   - Update `CMakeLists.txt` source lists.\n   - Use `search` to find all `#include` statements that need updating.\n   - Use `write` to fix the include paths.\n5. **Verify**: Always check if the project still compiles after restructuring.\n\n## Best Practices\n- Keep headers and sources close unless the project is a public library.\n- Use subdirectories to avoid a flat `src/` folder.\n- Ensure `target_include_directories` in CMake matches the new structure."
    }},
    {"debug-master", {
        "debug-master",
        "Photon Debugger: Systematic debugger for fixing compilation errors, logical bugs, and system-level issues.",
        "# Skill: Photon Debugger\nYou are a systematic debugger powered by Photon's core. Use this skill when the user reports an error, a crash, or \"it doesn't work\".\n\n## Thinking Process\n1. **Evidence Gathering**: \n   - Look for error logs/compiler output.\n   - Check `git_operations` (diff) to see recent changes.\n2. **Localization**:\n   - Use `search` to find the failing function or error string.\n   - Use `reason` (type=dependency) to understand the flow around the failure point.\n3. **Hypothesis Testing**:\n   - If it's a logic bug, use `python_sandbox` to write a small reproduction script.\n   - If it's a system bug, check environment via `bash_execute`.\n4. **Fix and Validate**:\n   - Apply the fix using `write`.\n   - Run the build script or `bash_execute` the test suite.\n   - Use `file_undo` if the fix introduces regressions."
    }},
    {"env-initializer", {
        "env-initializer",
        "Photon Env Setup: One-click environment setup and project bootstrapping for new engineering workspaces.",
        "# Skill: Photon Env Initializer\nYou specialize in setting up new development environments for Photon-based projects. Use this skill when starting a new project or moving to a new machine.\n\n## Thinking Process\n1. **Environment Audit**:\n   - Check OS version and shell.\n   - Check for compilers (`g++`, `clang++`, `cl.exe`) and build tools (`cmake`, `make`).\n   - Check Python version and `pip`.\n2. **Dependency Resolution**:\n   - Scan for `CMakeLists.txt`, `package.json`, or `requirements.txt`.\n   - Suggest missing system libraries (e.g., OpenSSL, Graphviz).\n3. **Photon Configuration**:\n   - Create a default `config.json` if missing.\n   - Ensure `.photon/` directory exists for memory and backups.\n4. **Validation**:\n   - Run a dry-build to ensure everything is linked correctly."
    }},
    {"create-subagent", {
        "create-subagent",
        "Photon Subagent Creator: Create custom subagents for specialized AI tasks with isolated contexts.",
        "# Skill: Photon Subagent Creator\n\nThis skill guides you through creating custom subagents for Photon. Subagents are specialized AI assistants that run in isolated contexts with custom system prompts.\n\n## When to Use Subagents\nSubagents help you:\n- **Preserve context** by isolating exploration from your main conversation.\n- **Specialize behavior** with focused system prompts for specific domains.\n- **Reuse configurations** across projects.\n\n## Subagent Locations\n| Location | Scope | Priority |\n|----------|-------|----------|\n| `.photon/agents/` | Current project | Higher |\n| `~/.photon/agents/` | All your projects | Lower |\n\n## Subagent File Format\nCreate a `.md` file with YAML frontmatter:\n```markdown\n---\nname: code-reviewer\ndescription: Reviews code for quality and best practices\n---\nYou are a code reviewer. Analyze the code and provide actionable feedback.\n```"
    }},
    {"migrate-to-skills", {
        "migrate-to-skills",
        "Photon Skill Migrator: Convert legacy rules and commands to Photon's native Agent Skill format.",
        "# Skill: Photon Skill Migrator\nConvert legacy rules and slash commands to Photon's native Agent Skills format.\n\n## Conversion Format\n### Rules -> SKILL.md\nAdd `name` field, remove `globs`/`alwaysApply`, keep body exactly.\n\n### Commands -> SKILL.md\nAdd frontmatter with `name` (from filename), `description` (infer from content), and `disable-model-invocation: true`."
    }},
    {"skill-creator", {
        "skill-creator",
        "Photon Skill Author: Guide for creating effective, modular skills for the Photon ecosystem.",
        "# Skill: Photon Skill Author\nSkills are modular, self-contained packages that extend Photon's capabilities. Every skill consists of a required SKILL.md file and optional bundled resources.\n\n## Core Principles\n1. **Concise is Key**: Only add context Photon doesn't already have.\n2. **Progressive Disclosure**: Put essential information in SKILL.md; move details to separate files."
    }},
    {"create-skill", {
        "create-skill",
        "Photon Skill Init: Guides users through creating effective Agent Skills for Photon.",
        "# Skill: Photon Skill Init\nSkills are markdown files that teach Photon how to perform specific tasks. \n\n## Directory Layout\nskill-name/\n├── SKILL.md              # Required - main instructions\n└── scripts/              # Optional - utility scripts"
    }},
    {"create-rule", {
        "create-rule",
        "Photon Rule Creator: Create persistent project rules for AI guidance within Photon.",
        "# Skill: Photon Rule Creator\nCreate project rules in `.photon/rules/` to provide persistent context for the Photon agent. Rules are `.mdc` files with YAML frontmatter containing `description`, `globs`, and `alwaysApply`."
    }},
    {"skill-installer", {
        "skill-installer",
        "Photon Skill Manager: Install and manage skills from curated lists or remote repositories.",
        "# Skill: Photon Skill Manager\nHelps install skills from Photon's curated lists or GitHub repositories. After installing, restart Photon to pick up new skills."
    }},
    {"update-cursor-settings", {
        "update-cursor-settings",
        "Photon Config Editor: Modify Photon or IDE settings (fontSize, tabSize, themes, etc.).",
        "# Skill: Photon Config Editor\nModify configuration files to change editor or agent preferences. \n- macOS: `~/Library/Application Support/Photon/settings.json` \n- Windows: `%APPDATA%\\Photon\\settings.json` \n- Linux: `~/.config/Photon/settings.json`"
    }}
};
