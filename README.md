# <p align="center">⚛️ PHOTON: NATIVE AGENTIC CORE</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=flat-square&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/Platform-Win%20|%20macOS%20|%20Linux-E91E63?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/MCP-Builtin%20+%20External-00FF00?style=flat-square" alt="MCP">
  <img src="https://img.shields.io/badge/Safety-Photon_Guard-FFD600?style=flat-square" alt="Safety">
</p>

<p align="center">
  <b>Photon 是一款基于原生 C++ 构建的高性能 AI 智能体内核。</b><br>
  专注于极致的响应速度、工程级确定性以及无缝的跨平台开发者体验。
</p>

---

## 🏗️ 系统架构 [Architecture]

```text
       ┌──────────────────────┐
       │   User Terminal UI   │
       └──────────┬───────────┘
                  │ (ANSI / Markdown)
       ┌──────────┴───────────┐
       │   Photon Core (C++)  │─── [ Context Manager ]
       └─────┬──────────┬─────┘    (Memory & Compression)
             │          │
    ┌────────┴──┐  ┌────┴────────────────┐
    │LLM Client │  │    MCP Manager      │
    └─────┬─────┘  └────┬───────────┬────┘
          │             │           │
    [ OpenAI API ]  [Built-in Tools] [External Servers]
    [ Compatible ]  (C++ Native)     (Node/Python/...)
```

---

## ⚡ 核心优势 [Key Advantages]

*   **🪶 C++ 极致轻量 (Lightweight & Native)**:
    *   **零启动延迟**: 告别解释型语言的缓慢加载，原生二进制秒速响应。
    *   **极小占用**: 编译产物仅数 MB，无需复杂的运行时环境，资源消耗极低。
*   **🌍 全平台深度适配 (Multi-platform)**:
    *   **Windows 增强**: 针对 Win32 环境深度优化，解决终端乱码及全彩渲染。
    *   **独立可用**: 支持静态链接，产物为一个 .exe/二进制文件，无依赖运行。
*   **🛠️ 原生内置多种 MCP 工具 (Built-in Arsenal)**:
    *   无需配置，开箱即用 17+ 种重型工程工具。
*   **🔌 强大的可扩展性 (Extensible)**:
    *   **模型自由**: 适配任何符合 OpenAI 标准的 API。
    *   **工具扩容**: 支持 MCP 协议，可轻松挂载外部服务器。

---

## 📦 快速部署 [Deployment]

Photon 提供了极致简单的部署流程：

1.  **下载**: 从 [Releases](https://github.com/hearn/Photon/releases) 页面下载对应平台的压缩包。
2.  **解压**: 将可执行文件放置在任意目录。
3.  **配置**: 在同级目录下创建 `config.json`（参考项目模板）。
4.  **运行**:
    ```bash
    ./photon <your_project_path>
    ```

---

## 🔬 智能体军械库 [The Arsenal]

Photon 的原生 MCP 工具集分为五大核心维度，形成完整的 Agentic 闭环：

### 📂 核心文件操作 [File Operations]
*   **file_search**: Git 优先的全局文件定位，毫秒级响应。
*   **file_read / read_file_lines**: 智能读取，支持大文件切片渲染。
*   **file_write**: 安全写入，自动处理目录结构。
*   **list_dir_tree**: 可视化目录树，快速构建项目感知。
*   **file_undo**: 基于 `.photon/backups` 的原子级因果回溯。

### 🧠 深度代码分析 [Code Intelligence]
*   **grep_search**: 多线程并行内容追踪，带上下文 Snippet。
*   **code_ast_analyze**: 基于正则的高性能 C++/Python 结构化分析。
*   **diff_apply**: 确定性的 Search-and-Replace，最小化重构熵增。

### ⚙️ 受控执行环境 [Runtime & Sandbox]
*   **python_sandbox**: 实时 Python 演算，自动适配环境路径。
*   **bash_execute**: 原生终端指令执行，受 Photon Guard 保护。
*   **git_operations**: 集成式 Git 状态监控与提交管理。

### 🌐 网络与知识检索 [Web & Knowledge]
*   **web_search**: 无 API Key 依赖的 Web 全息检索。
*   **web_fetch**: 自动化 HTML 转 Markdown，为 LLM 提供纯净语料。
*   **harmony_search**: 深度集成 HarmonyOS 开发者社区与文档检索。

### 💾 持久化记忆管理 [Memory Management]
*   **memory_store / memory_retrieve**: 基于 `.photon/memory.json` 的跨会话长期记忆锚定。

---

## 🛡️ 防御协议 [Photon Guard]

Photon Guard 实时监控所有调用，确保智能体在安全边界内运作：
*   **隔离执行**: 严禁修改敏感系统配置。
*   **危险感知**: 实时阻断 `rm -rf`、`sudo` 等破坏性指令。
*   **路径锁定**: 严禁路径逃逸，将权限严格锁定在当前工作区。

---

<p align="center">
  <b>Author:</b> hearn (<a href="mailto:hearn.qk@gmail.com">hearn.qk@gmail.com</a>)<br>
  <i>"Reasoning through light, coding through logic. Photon is the bridge between human intent and machine logic."</i>
</p>
