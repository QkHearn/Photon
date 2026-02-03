# <p align="center">⚛️ PHOTON</p>
<p align="center">
  <i align="center">原生智能体内核 — 为工程的未来而生</i>
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
    <img src="https://img.shields.io/badge/联系-hearn.qk@gmail.com-red?style=flat-square&logo=gmail" alt="Email">
  </a>
</p>

---

<p align="center">
  <img src="./demo.png" width="600" alt="Photon Demo">
</p>

---

## 💎 为什么选择 Photon?

当通用 AI 智能体依赖"模糊"文本搜索时,Photon 为**工程精度**而生。

### 🚀 硬核原生性能
*   **零延迟**: 基于 C++17 构建。工具链调用和项目索引以微秒计,而非秒级。
*   **最小占用**: 仅需 10-30MB 内存运行。无需笨重的 Node.js 或 Python 运行时。
*   **单一二进制**: 可移植、自包含的引擎,随处可运行——从本地终端到远程 CI/CD 管道。

### 🎯 外科手术般的精度 (AST + LSP)
*   **超越 Grep**: 不像只能基于文本"猜测"的工具,Photon 使用 **Tree-sitter** 进行实时 AST 解析。它精确理解函数和类的作用域(精确到行范围)。
*   **编译器级智能**: 深度 **LSP** 集成提供 100% 准确的"跳转到定义"和类型感知分析。
*   **行级控制**: 使用 `file_edit_lines` 以外科手术般的精度修改代码,在提交任何更改前提供实时 **Git Diff 预览**。

### 🧠 智能上下文管理
*   **Token 级精准读取**: 永不浪费 Token 在 10,000 行文件上。Photon 生成**结构化摘要**并使用**窗口化上下文读取**只给 AI 它需要的内容。
*   **神经持久化**: 与项目特定编码标准和历史上下文对齐的多态记忆系统。

### 🛡️ 安全优先的智能体循环
*   **自主推理**: 多步骤**智能体循环**,包括计划、执行、验证和反思。
*   **人在回路中**: 高风险操作(如 shell 执行或批量编辑)需要明确确认,让你完全掌控。
*   **确定性撤销**: 每次修改都有备份。如果 AI 出错,一条命令即可恢复到安全状态。

---

## 🏗️ 架构

```text
                     ┌──────────────────────────────────────────┐
                     │           IDE / 终端宿主                  │
                     │ (VS Code, JetBrains, Cursor, Vim 等)     │
                     └───────────────────┬──────────────────────┘
                                         │ (标准 I/O)
                     ┌───────────────────▼──────────────────────┐
                     │         PHOTON 内核 (C++)                │
                     │  ──────────────────────────────────────  │
                     │  [ 上下文管理器 ]    [ 记忆系统 ]        │
                     │  (短期记忆)          (JSON/Markdown)     │
                     └───────────┬────────────────────┬─────────┘
                                 │                    │
                    ┌────────────▼───────┐    ┌───────▼─────────────┐
                    │     LLM 客户端     │    │  MCP 工具管理器     │
                    │   (推理中枢)       │    │   (执行中枢)        │
                    └────────────────────┘    └───────┬─────────────┘
                                                      │
          ┌───────────────────────────────────────────┴──────────────────────────┐
          │                                    │                                 │
┌─────────▼──────────┐               ┌─────────▼──────────┐            ┌─────────▼──────────┐
│   内置工具         │               │   专家技能          │            │    外部 MCP        │
│    (C++ 原生)      │               │  (模块化逻辑)       │            │   (Node / Python)  │
├────────────────────┤               ├────────────────────┤            ├────────────────────┤
│• 并行搜索          │               │• 代码架构师         │            │• Puppeteer         │
│• AST 分析          │               │• 调试大师           │            │• Google 搜索       │
│• Python 沙箱       │               │• 环境初始化器       │            │• 自定义服务器      │
└────────────────────┘               └────────────────────┘            └────────────────────┘
```

---

## 🧠 核心概念

### 🛠️ 工具 (Tools): "数字触手" (执行层)

**工具是 AI 与物理世界交互的唯一方式**。没有工具,AI 只是聊天框;有了 Photon 的工具,它就是功能性工程师。

*   **全息感知**: 内置高性能并行检索和 AST 分析,使 AI 能够在不浪费 Token 的情况下解构大规模项目结构。
*   **原子干预**: 通过确定性代码注入 (Diff Apply) 和隔离沙箱确保每次执行都精确且安全。
*   **无限扩展**: 基于 **MCP (模型上下文协议)**,支持全球开发者社区的数千个工具服务器。

### 📜 技能 (Skills): "专家本能" (专家范式)

**通用 LLM 在特定领域往往缺乏工程深度**。技能系统通过将"经验"转化为"本能"来弥合这一差距。

#### 🎯 **动态激活机制** (工业级设计)

Photon 采用**运行时按需激活、Just-In-Time 注入**的 Skill 管理策略:

```
配置层 (config.json)
  ↓ 定义 Allowlist (允许列表)
启动时
  ↓ 加载 Skill 元数据,注入发现列表
运行时 (LLM 决策)
  ↓ 调用 skill_activate(name)
激活后
  ↓ 动态注入工具、约束、接口
```

**核心优势**:
- ✅ **Token 效率**: 节省 80-97% Token 成本
- ✅ **可扩展性**: 支持 200+ Skill (vs 全量注入 ~20 个)
- ✅ **智能决策**: LLM 自主按需激活
- ✅ **安全可控**: Allowlist + Runtime 双重验证

**详细文档**: 查看 [Skill 动态激活教程](./docs/tutorials/README.md)

*   **逻辑编码化**: 模块化 `SKILL.md` 文件将资深架构师的思维过程(如性能调优、高并发设计)直接注入 AI 的决策层。
*   **主动进化**: AI 在执行前主动检索和阅读相关技能指南,确保输出是工业级工程实践而非简单概率预测。

### 💾 记忆 (Memory): "神经持久化" (自我延续性)

**没有记忆的 AI 只是单次函数调用;有记忆的 AI 是不断进化的生命**。

*   **多态记忆**: 结合 JSON 碎片化事实(快速检索)和 Markdown 结构化知识(深度理解)。
*   **习惯对齐**: Photon 持续记录你的项目标准、历史 Bug 教训和特定编码偏好。随着时间推移,从"协作"转向"同步"。

### 📑 上下文 (Context): "认知工作空间" (高维思考)

**LLM 上下文窗口既昂贵又有限**。上下文管理器是 Photon 的"大脑管家"。

*   **智能修剪与压缩**: 当对话爆炸时,内置上下文管理器识别关键决策点并使用 LLM 进行递归摘要压缩。
*   **无损推理**: 在显著延长 AI 有效思考长度的同时保持核心逻辑和历史决策,使其能够处理涉及数万行代码的大规模任务。

---

## 🚀 快速开始

### 1. 下载
从 [Releases](https://github.com/QkHearn/Photon/releases) 页面下载适合你操作系统的二进制文件。

### 2. 配置
在程序同一目录下创建或编辑 `config.json`:

```json
{
  "llm": {
    "api_key": "YOUR_API_KEY",
    "base_url": "https://api.moonshot.cn/v1",
    "model": "kimi-k2-0905-preview"
  },
  "agent": {
    "skill_roots": [
      "~/.photon/skills",
      "./builtin_skills"
    ]
  }
}
```

### 3. 启动
在终端中直接运行,指向你的项目目录:

```bash
# 授予执行权限 (Linux/macOS)
chmod +x photon

# 开始分析
./photon /path/to/your/project
```

### 4. LLM 请求适配器 (Kimi 等)

某些提供商(如 Kimi)返回的消息格式在下次请求中不被接受。Photon 在每次发送前规范化消息,确保请求有效。这是**设计**,而非权宜之计。

- **`normalizeForKimi(messages)`**: 将 assistant 的 `content` 从数组展平为字符串;从 tool 消息中移除 `name`。详见 [docs/llm-adapters.md](docs/llm-adapters.md)。

---

## 📚 文档

### 核心文档

- [Constitution v2.0](./photon_agent_constitution_v_2.md) - Agent 行为规范
- [重构总结](./REFACTOR_SUMMARY.md) - 架构重构记录
- [设计文档](./design.md) - 系统设计详解

### 教程

- [Skill 动态激活完整教程](./docs/tutorials/README.md) ⭐ **推荐阅读**
  - 快速参考 (5 分钟)
  - 完整总结 (15 分钟)
  - 设计文档 (30 分钟)
  - 使用示例 (20 分钟)
  - 实现清单 (10 分钟)

### 技术细节

- [LLM 适配器](./docs/llm-adapters.md) - LLM 消息格式适配
- [MCP 集成](./docs/mcp-integration.md) - 模型上下文协议集成

---

## 🎯 核心特性

### ✅ 已实现

- ✅ **工具系统**: 基于 MCP 的可扩展工具架构
- ✅ **Skill 系统**: 动态激活、Just-In-Time 注入
- ✅ **记忆系统**: 项目记忆 + 失败记忆
- ✅ **上下文管理**: 智能压缩和修剪
- ✅ **LSP 集成**: 精确符号导航
- ✅ **AST 分析**: Tree-sitter 深度集成
- ✅ **安全机制**: 人在回路 + 确定性撤销

### 🚧 开发中

- 🚧 **Agent 运行时**: Plan-Act-Observe 循环完善
- 🚧 **Skill 元数据解析**: frontmatter 完整解析
- 🚧 **性能优化**: 并行执行优化

---

## 🏆 设计哲学

### 工程优先

Photon 不是通用聊天机器人,而是**工程执行引擎**:

- **精度 > 灵活性**: 编译器级的准确性,而非"差不多"
- **性能 > 易用性**: 原生 C++ 性能,零妥协
- **确定性 > 概率性**: 可预测的行为,可靠的结果

### 工业级架构

- **三层分离**: 配置层 → 加载层 → 激活层
- **按需加载**: Token 效率优先
- **安全可控**: Allowlist + Runtime 验证
- **可观测性**: 完整的激活/执行日志

---

## 📊 性能指标

### Token 效率

| Skill 数量 | 全量注入 | 动态激活 | 节省 |
|-----------|---------|---------|------|
| 10 | 5K | 1K | 80% |
| 20 | 10K | 2K | 80% |
| 50 | 25K | 2.5K | 90% |
| 200 | 100K | 3K | 97% |

### 运行时性能

- **启动时间**: < 100ms
- **工具调用延迟**: < 1ms
- **AST 解析**: < 10ms (10,000 行代码)
- **内存占用**: 10-30MB

---

## 🤝 贡献

欢迎贡献! 请查看 [CONTRIBUTING.md](CONTRIBUTING.md) 了解详情。

### 贡献领域

- 🔧 新工具实现
- 📜 Skill 开发
- 📖 文档改进
- 🐛 Bug 修复
- ✨ 功能增强

---

## 📜 许可证

MIT License - 详见 [LICENSE](LICENSE)

---

## 🔗 相关链接

- [GitHub](https://github.com/QkHearn/Photon)
- [文档](./docs/README.md)
- [教程](./docs/tutorials/README.md)
- [问题反馈](https://github.com/QkHearn/Photon/issues)

---

<p align="center">
  <i>"Photon: 以光速连接人类意图与机器逻辑"</i>
</p>

<p align="center">
  Made with ⚛️ by <a href="https://github.com/QkHearn">Hearn</a>
</p>
