# <p align="center">⚛️ PHOTON: THE AUTONOMOUS AGENT</p>

<p align="center">
  <img src="https://img.shields.io/badge/Status-Agentic_v1.5-00FF00?style=flat-square" alt="Status">
  <img src="https://img.shields.io/badge/Engine-C%2B%2B17_Native_Agent-00599C?style=flat-square&logo=c%2B%2B" alt="Engine">
  <img src="https://img.shields.io/badge/Logic-Autonomous_Reasoning-E91E63?style=flat-square" alt="Logic">
  <img src="https://img.shields.io/badge/Safety-Photon_Guard-FFD600?style=flat-square" alt="Safety">
</p>

<p align="center">
  <b>「 观测、决策、执行。Photon 不仅是工具，更是具备自主意识的逻辑实体。 」</b><br>
  <i>原生 C++ 驱动的超高性能 AI 智能体 · 自主思考、精准干涉、跨域协同</i>
</p>

---

## 🧠 智能体核心 [The Agentic Core]

Photon 并非简单的脚本集合，它是一个完整的 **自主智能体 (Autonomous Agent)** 系统，旨在复杂的工程环境中模拟人类专家的思维与操作：

*   **感知层 (Sensing)**: 
    *   **Git-Native Perception**: 深度集成 Git 内核，通过 `ls-files` 与 `grep` 瞬间感知万级文件变动。
    *   **Context Awareness**: 智能管理上下文窗口，在压缩与遗忘中寻找平衡，保持长效的思维连贯性。
*   **认知层 (Cognition)**:
    *   **Recursive Reasoning**: 基于 LLM 的多步推理，能够自主拆解复杂任务并选择最优路径。
    *   **Nexus Memory**: 跨会话的持久化记忆能力，将关键知识沉淀于 `.photon/memory.json`。
*   **行动层 (Acting)**:
    *   **Cross-Domain Execution**: 在 Python 沙箱、Bash 终端与 Web 全息世界之间自由切换。
    *   **Atomic Intervention**: 每一行代码的修改都是确定性的原子操作，支持 `undo` 级别的因果回溯。

---

## ⚡ 核心竞争力 [Agent Competitiveness]

*   **🚀 降维打击级的响应**: 
    基于 C++ 17 原生构建，消除了 Python/Node 智能体的启动延迟。Agent 的决策毫秒级下达，行动瞬间完成。
*   **🛡️ Photon Guard 协议**:
    智能体安全防护层。自动识别并阻断高危系统调用、路径逃逸及非法指令，确保智能体在安全范围内运作。
*   **📦 绝对独立的单体化**:
    深度静态链接，无 DLL 依赖。一个体积仅几 MB 的可执行文件，即可承载完整、强大的 Agentic 军械库。

---

## 🔬 智能体军械库 [The Arsenal]

Photon 内置了 17 个高度集成的智能组件，构成了完整的 Agentic 闭环：

| 模块 ID | 等级 | Agentic 效能 [Capabilities] |
| :--- | :--- | :--- |
| **file_search** | `Σ` | **感知锚点**：基于 Git 的极速定位，为智能体提供全局视野。 |
| **grep_search** | `Ω` | **深度溯源**：多线程内容追踪，在海量源码中寻找逻辑关联。 |
| **diff_apply** | `Δ` | **精准重构**：自主干涉源码，确保修改的最小冲突与最大成功率。 |
| **python_sandbox** | `Π` | **计算演算**：智能体专用的逻辑实验室，用于复杂计算与动态脚本验证。 |
| **web_intelligence**| `Φ` | **外部认知**：全息 Web 抓取与专业社区检索，打破知识边界。 |
| **nexus_memory** | `μ` | **经验沉淀**：将多次任务的成功经验固化为智能体的长期知识。 |

---

## 🛠️ 部署序列 [Deployment]

Photon 是自包含的智能体实体，支持 Windows/macOS/Linux 跨平台物化。

### 构建命令
```bash
# Windows (使用 x64-windows-static 静态构建)
cmake -B build -DVCPKG_TARGET_TRIPLET=x64-windows-static && cmake --build build --config Release

# macOS / Linux (一键编译)
./build.sh
```

---

## 🛡️ 防御协议 [Safety Protocols]

Photon Guard 为智能体设置了牢不可破的边界：
*   **隔离执行**: `python_sandbox` 受限运行，严禁修改系统配置。
*   **危险感知**: 实时扫描 `rm -rf`、`sudo`、`reg` 等破坏性指令并自动掐断。
*   **路径隔离**: 严禁任何形式的路径逃逸，将智能体权限严格锁定在当前工作区。

---

<p align="center">
  <b>Author:</b> hearn (<a href="mailto:hearn.qk@gmail.com">hearn.qk@gmail.com</a>)<br>
  <i>"I think, therefore I code. Photon is the bridge between human intent and machine logic."</i>
</p>
