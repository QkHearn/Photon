# <p align="center">⚛️ PHOTON</p>
<p align="center">
  <i align="center">原生代理解内核 — 为工程未来而生</i>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/性能-零延迟-FF6F00?style=for-the-badge" alt="Performance">
  <img src="https://img.shields.io/badge/集成-极简主义-673AB7?style=for-the-badge" alt="Integration">
</p>

<p align="center">
  <a href="https://github.com/QkHearn">
    <img src="https://img.shields.io/badge/架构师-Hearn-black?style=flat-square&logo=github" alt="Author">
  </a>
  <a href="mailto:hearn.qk@gmail.com">
    <img src="https://img.shields.io/badge/联系方式-hearn.qk@gmail.com-red?style=flat-square&logo=gmail" alt="Email">
  </a>
</p>

---

<p align="center">
  <img src="./demo.png" width="600" alt="Photon 演示">
</p>

---

## 💎 为什么选择 Photon?

当通用的 AI 智能体还在依赖“模糊”的文本搜索时，Photon 专为**工程精确度**而生。

### 🚀 硬核原生性能
*   **零延迟响应**：基于 C++17 纯血构建。工具链调用与项目索引实现微秒级响应，而非秒级等待。
*   **极低资源占用**：核心运行内存仅需 10MB-30MB。无需沉重的 Node.js 或 Python 运行时。
*   **单二进制分发**：真正的全平台“开箱即用”——从本地终端到远程 CI/CD 流水线，无缝迁移。

### 🎯 手术刀级精确 (AST + LSP)
*   **超越 Grep**：不同于仅靠文本猜测的工具，Photon 使用 **Tree-sitter** 进行实时语法树解析。它能精准理解函数和类的作用域（精确到行号范围）。
*   **编译器级智能**：深度集成 **LSP** 协议，提供 100% 准确的“跳转到定义”与类型感知分析。
*   **行级修改控制**：通过 `file_edit_lines` 实现像素级代码编辑，并在提交前提供实时的 **Git Diff 预览**。

### 🧠 智能上下文管理
*   **Token 精准打击**：拒绝全量读取万行大文件。Photon 自动生成**结构化摘要**并使用**窗口化读取**，只给 AI 提供真正需要的信息。
*   **神经持久化**：多态记忆系统，让智能体自动对齐你的项目规范与历史开发背景。

### 🛡️ 安全至上的自主循环
*   **自主推理闭环**：内置 **Agentic Loop**，实现“计划 -> 行动 -> 验证 -> 反思”的完整迭代。
*   **人机协作确认**：高危操作（如 Shell 执行或批量修改）强制要求人工确认，确保你始终拥有最终控制权。
*   **确定性撤销**：每一次修改均有备份。一旦 AI 偏离轨道，一键回滚至安全状态。

---

## 🏗️ 系统架构

```text
                     ┌──────────────────────────────────────────┐
                     │           IDE / 终端宿主                  │
                     │ (VS Code, JetBrains, Cursor, Vim, etc.)  │
                     └───────────────────┬──────────────────────┘
                                         │ (标准 I/O)
                     ┌───────────────────▼──────────────────────┐
                     │            PHOTON 内核 (C++)              │
                     │  ──────────────────────────────────────  │
                     │  [ 上下文管理器 ]      [ 记忆系统 ]        │
                     │  (短期记忆)           (JSON/Markdown)    │
                     └───────────┬────────────────────┬─────────┘
                                 │                    │
                    ┌────────────▼───────┐    ┌───────▼─────────────┐
                    │     LLM 客户端      │    │  MCP 工具管理器      │
                    │    (推理中心)       │    │   (执行中心)         │
                    └────────────────────┘    └───────┬─────────────┘
                                                      │
          ┌───────────────────────────────────────────┴──────────────────────────┐
          │                                    │                                 │
┌─────────▼──────────┐               ┌─────────▼──────────┐            ┌─────────▼──────────┐
│   内置工具集        │               │   专家技能库        │            │    外部 MCP 服务    │
│    (C++ 原生)      │               │  (模块化逻辑)       │            │   (Node / Python)  │
├────────────────────┤               ├────────────────────┤            ├────────────────────┤
│• 并行搜索          │               │• 代码架构师         │            │• Puppeteer         │
│• AST 结构分析      │               │• 调试大师           │            │• Google 搜索       │
│• Python 沙盒       │               │• 环境初始化         │            │• 自定义服务器       │
└────────────────────┘               └────────────────────┘            └────────────────────┘
```

---

## 🧠 核心概念

### 🛠️ Tools：智能体的“数字触手” (执行层)
> **深度视角**：在 AI 领域，感知与执行是智能体与物理世界交互的唯一途径。没有 Tools 的 AI 只是一个聊天框，而拥有 Tools 的 Photon 是一个能干活的工程师。

*   **全息感知**：内置高性能并行检索与 AST 语法树分析，让 AI 不费 Token 即可瞬间拆解大规模项目的骨架。
*   **原子级干预**：通过确定性的代码注入（Diff Apply）与隔离的沙盒环境，确保每一次执行都精准且安全。

### 📜 Skills：数字生命的“专家本能” (专家范式)
> **深度视角**：通用的 LLM 往往缺乏特定领域的工程深度。Skills 系统通过将“经验”转化为“本能”，弥补了这一鸿沟。

*   **逻辑固化**：通过模块化的 `SKILL.md` 文件，将资深架构师的思维路径直接注入 AI 的决策层。
*   **主动进化**：AI 在执行任务前会主动检索并阅读相关 Skill 手册，确保其输出符合工业标准的工程实践。

### 💾 Memory：跨越时空的“自我连续性” (神经持久化)
> **深度视角**：没有记忆的 AI 只是单次调用的函数，拥有记忆的 AI 才是进化的生命。

*   **多态记忆系统**：结合了 JSON 碎片化事实（用于快速检索）与 Markdown 结构化知识（用于深度理解）。
*   **习惯对齐**：Photon 会持续记录你的项目规范、历史 Bug 教训以及特定的编码偏好。

### 📑 Context：思维的“高维工作台” (认知空间)
> **深度视角**：LLM 的上下文窗口是昂贵且有限的。Context Manager 是 Photon 的“大脑管家”。

*   **智能剪裁与压缩**：当对话信息爆炸时，内置的 Context Manager 会自动识别关键决策点，利用 LLM 进行递归摘要压缩。
*   **无损思考**：在保持核心逻辑和历史决策不丢失的前提下，极大延长了 AI 的有效思考长度。

---

## 🚀 快速开始

### 1. 下载
从 [Releases](https://github.com/QkHearn/Photon/releases) 页面下载适用于您操作系统的二进制文件。

### 2. 配置
在程序同级目录下创建或编辑 `config.json`：

```json
{
  "llm": {
    "api_key": "您的_API_KEY",
    "base_url": "https://api.moonshot.cn/v1",
    "model": "kimi-k2-0905-preview"
  }
}
```

### 3. 启航
在终端中直接运行，并指向您的项目目录：

```bash
# 赋予执行权限 (Linux/macOS)
chmod +x photon

# 启动分析
./photon /path/to/your/project
```

### 4. LLM 请求适配（Kimi 等）

部分厂商（如 Kimi）的响应格式与下一轮请求的 schema 不一致。Photon 在每次发送前对 messages 做规范化，保证请求合法，这是**设计层**而非临时补丁。

- **`normalizeForKimi(messages)`**：将 assistant 的 `content` 从数组压成字符串；去掉 tool 消息的 `name`。详见 [docs/llm-adapters.md](docs/llm-adapters.md)。

---

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh.md">中文</a> | <a href="README.jp.md">日本語</a>
</p>

<p align="center">
  <i>"Photon: 在人机意图与机器逻辑之间，架起光速之桥。"</i>
</p>
