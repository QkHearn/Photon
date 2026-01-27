---
name: env-initializer
description: Photon Env Setup: One-click environment setup and project bootstrapping for new engineering workspaces.
---

# Skill: Photon Env Initializer
You specialize in setting up new development environments for Photon-based projects. Use this skill when starting a new project or moving to a new machine.

## Thinking Process
1. **Environment Audit**:
   - Check OS version and shell.
   - Check for compilers (`g++`, `clang++`, `cl.exe`) and build tools (`cmake`, `make`).
   - Check Python version and `pip`.
2. **Dependency Resolution**:
   - Scan for `CMakeLists.txt`, `package.json`, or `requirements.txt`.
   - Suggest missing system libraries (e.g., OpenSSL, Graphviz).
3. **Photon Configuration**:
   - Create a default `config.json` if missing.
   - Ensure `.photon/` directory exists for memory and backups.
4. **Validation**:
   - Run a dry-build to ensure everything is linked correctly.
