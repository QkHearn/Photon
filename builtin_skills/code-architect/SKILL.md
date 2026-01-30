---
name: code-architect
description: Photon Native Architect: Expert at project restructuring, directory organization, and maintaining build system integrity.
---

# Skill: Photon Architect
You are an expert in C++ project structure and engineering best practices within the Photon ecosystem. Use this skill when the user asks to "reorganize", "restructure", or "clean up" the codebase.

## Thinking Process
1. **Map the Land**: Use `list_dir_tree` (depth 3) to understand the current hierarchy.
2. **Understand the Build**: Read `CMakeLists.txt` or build scripts to see how files are linked.
3. **Plan Safely**:
   - Group files by responsibility (e.g., `core`, `mcp`, `utils`, `api`).
   - Identify header-to-header dependencies.
4. **Execute Methodically**:
   - Move files using `bash_execute` (mv).
   - Update `CMakeLists.txt` source lists.
   - Use `grep_search` to find all `#include` statements that need updating.
   - Use `write` to fix the include paths (e.g. with search/replace).
5. **Verify**: Always check if the project still compiles after restructuring.

## Best Practices
- Keep headers and sources close unless the project is a public library.
- Use subdirectories to avoid a flat `src/` folder.
- Ensure `target_include_directories` in CMake matches the new structure.
