# <p align="center">⚛️ PHOTON: THE NATIVE AGENTIC CORE</p>

<p align="center">
  <b>Author:</b> hearn (<a href="mailto:hearn.qk@gmail.com">hearn.qk@gmail.com</a>)
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=flat-square&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/Performance-Zero_Latency-FF6F00?style=flat-square" alt="Performance">
  <img src="https://img.shields.io/badge/Reasoning-Sequential_Thinking-673AB7?style=flat-square" alt="Reasoning">
  <img src="https://img.shields.io/badge/Platform-Win_|_macOS_|_Linux-E91E63?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Safety-Photon_Guard-FFD600?style=flat-square" alt="Safety">
</p>

<p align="center">
  <a href="README.md">中文</a> | <a href="README.en.md">English</a> | <a href="README.jp.md">日本語</a>
</p>

<p align="center">
  <b>"Reasoning through light, coding through logic."</b><br>
  Photon 不仅仅是一个 AI 助手，它是基于原生 C++ 构建的<b>高性能、工程级自进化智能体内核</b>。<br>
  它赋予 AI 真正的“手”与“大脑”，通过微秒级的响应速度、深度推理能力与多维感知系统，<br>
  <b>彻底重定义 AI 驱动的软件开发范式。</b>
</p>

<p align="center">
  <img src="demo.png" alt="Photon Demo" width="800">
</p>

---

## 📦 快速开始 [Quick Start]

1.  **下载**: 从 [Releases](https://github.com/QkHearn/Photon/releases) 提取二进制产物。
2.  **配置**: 设置 `config.json` 中的 API Key 与模型参数。
3.  **启航**:
    ```bash
    ./photon <your_project_path>
    ```

---

## 🌟 核心进化 [Core Evolutions]

### 🧠 深度认知推理 (Cognitive Reasoning)
Photon 拒绝“直觉式应答”。面对复杂工程挑战，它自动开启内部**思考画布 (Sequential Thinking)**，进行多阶假设建立、逻辑推演与策略修正。它像资深工程师一样在“大脑”中反复打磨方案，确保每一次执行都具备工程级的严谨性与可行性。

### 🏎️ 极致原生性能 (Native Velocity)
告别解释型语言的迟钝。基于 **C++17 纯血构建**，Photon 的工具链调用达到**微秒级**响应：
*   **零秒启动**: 二进制程序秒速进入作战状态。
*   **并发扫描**: 拥有比传统 IDE 更快的内容定位能力，毫秒级扫描万行代码库，精准定位上下文。

### 🧩 模块化技能系统 (Skill Modules)
Photon 引入了先进的 **Skill 模块化架构**。不仅仅是工具的堆砌，而是**能力的可插拔**。
*   **Skill Manager**: 动态加载专门领域的知识包（如 `skill-creator`），瞬间让通用 AI 变身特定领域的专家。
*   **本地化同步**: 自动将全局技能同步至项目本地，支持项目级的能力定制与隔离。

### 👁️ 多维架构感知 (Architecture Perception)
Photon 拥有超越文本的感知力。它不仅能通过 **AST 分析** 瞬间拆解复杂项目的骨架，更能利用 **Graphviz** 将抽象的代码逻辑实时渲染为**可视化架构图**，让系统脉络一目了然。

### 🕸️ 无限边界扩展 (Universal MCP)
通过标准 **Model Context Protocol (MCP)**，Photon 连接了整个现代生态：
*   **浏览器霸权**: 完美集成 Puppeteer，赋予 AI 直接接管 Chrome 的能力（查阅文档、调试网页、全自动化流程）。
*   **双域演算**: 结合 C++ 的稳定执行与 Python Sandbox 的灵活演算，覆盖全场景需求。

---

## 🏗️ 系统架构 [Architecture]

```text
       ┌──────────────────────┐
       │   User Terminal UI   │  (Rich ANSI / Markdown Rendering)
       └──────────┬───────────┘
                  │ 
       ┌──────────┴───────────┐
       │   Photon Core (C++)  │─── [ Context Manager ]
       └─────┬──────────┬─────┘    (Memory & Adaptive Compression)
             │          │
    ┌────────┴──┐  ┌────┴─────────────────────────────┐
    │LLM Client │  │         MCP Tool Manager         │
    └─────┬─────┘  └────┬────────────┬────────────────┤
          │             │            │                │
    [ API Gateway ]  [Built-in]   [External]       [Skills]
    (OpenAI / Kimi)  (C++ Native) (Node / Python)  (Modular Caps)
```

---

## 🔬 智能体军械库 [The Arsenal]

Photon 预装了 **20+** 种重型工程工具，形成完整的感知-决策-执行闭环：

### 📂 全息文件感知
*   **file_search / grep_search**: Git 优先的并行检索，精准锁定代码片段。
*   **read_file_lines**: 智能切片读取，在保护 Token 窗口的同时获取最深细节。
*   **code_ast_analyze**: 骨架感知，不费 Token 即可理解全项目结构。

### 🛠️ 原子级重构与执行
*   **diff_apply**: 确定性的代码注入，确保重构过程零冗余。
*   **python_sandbox**: 自动识别 `.venv` 环境，支持即时安装依赖并执行演算。
*   **arch_visualize**: 将代码逻辑一键转化为架构图。
*   **skill_read**: 动态读取技能指南，按需加载专业知识。

### 🌍 全球知识检索
*   **web_search / web_fetch**: 自动化信息采集，为 AI 提供最纯净的实时语料。
*   **harmony_search**: 深度集成鸿蒙开发者生态，秒级定位 HarmonyOS 核心文档。

### 💾 跨会话持久记忆
*   **memory_management**: 记录项目规范与历史教训，Photon 会随着使用变得越来越懂你的习惯。

---

## 🛡️ 防御协议 [Photon Guard]

**Photon Guard** 实时监控所有底层调用，确保智能体在安全边界内运作：
*   **路径锁定**: 严禁路径逃逸，权限严格限制在当前工作区。
*   **危险感知**: 实时阻断 `rm -rf`、`sudo` 等破坏性指令。
*   **隔离执行**: 敏感系统操作强制拦截。

---

<p align="center">
  <i>"Photon is the bridge between human intent and machine logic."</i>
</p>
