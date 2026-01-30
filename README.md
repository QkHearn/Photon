# <p align="center">⚛️ PHOTON</p>
<p align="center">
  <i align="center">The Native Agentic Core — Built for the Future of Engineering</i>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++17">
  <img src="https://img.shields.io/badge/Performance-Zero_Latency-FF6F00?style=for-the-badge" alt="Performance">
  <img src="https://img.shields.io/badge/Integration-Minimalist-673AB7?style=for-the-badge" alt="Integration">
</p>

<p align="center">
  <a href="https://github.com/QkHearn">
    <img src="https://img.shields.io/badge/Architect-Hearn-black?style=flat-square&logo=github" alt="Author">
  </a>
  <a href="mailto:hearn.qk@gmail.com">
    <img src="https://img.shields.io/badge/Contact-hearn.qk@gmail.com-red?style=flat-square&logo=gmail" alt="Email">
  </a>
</p>

---

<p align="center">
  <img src="./demo.png" width="600" alt="Photon Demo">
</p>

---

## 💎 Why Photon?

While generic AI agents rely on "fuzzy" text searching, Photon is built for **engineering precision**.

### 🚀 Hardcore Native Performance
*   **Zero Latency**: Built with C++17. Toolchain calls and project indexing happen in microseconds, not seconds.
*   **Minimal Footprint**: Runs on just 10MB-30MB of RAM. No heavy Node.js or Python runtimes required.
*   **Single Binary**: A portable, self-contained engine that works everywhere—from your local terminal to remote CI/CD pipelines.

### 🎯 Surgical Precision (AST + LSP)
*   **Beyond Grep**: Unlike tools that just "guess" based on text, Photon uses **Tree-sitter** for real-time AST parsing. It understands the exact scope of your functions and classes (down to the exact line range).
*   **Compiler-Grade Intelligence**: Deep **LSP** integration provides 100% accurate "Go to Definition" and type-aware analysis.
*   **Line-Level Control**: Modify code with surgical accuracy using `file_edit_lines`, featuring real-time **Git Diff previews** before any changes are committed.

### 🧠 Intelligent Context Management
*   **Token-Surgical Reads**: Never waste tokens on 10,000-line files. Photon generates **Structural Summaries** and uses **Windowed Context Reads** to give the AI only what it needs.
*   **Neural Persistence**: A polymorphic memory system that aligns with your project's specific coding standards and historical context.

### 🛡️ Safety-First Agentic Loop
*   **Autonomous Reasoning**: A multi-step **Agentic Loop** that plans, acts, verifies, and reflects.
*   **Human-in-the-Loop**: High-risk actions (like shell execution or bulk edits) require explicit confirmation, keeping you in total control.
*   **Deterministic Undo**: Every modification is backed up. If the AI makes a mistake, one command brings you back to a safe state.

---

## 🏗️ Architecture

```text
                     ┌──────────────────────────────────────────┐
                     │           IDE / Terminal Host            │
                     │ (VS Code, JetBrains, Cursor, Vim, etc.)  │
                     └───────────────────┬──────────────────────┘
                                         │ (Standard I/O)
                     ┌───────────────────▼──────────────────────┐
                     │            PHOTON KERNEL (C++)           │
                     │  ──────────────────────────────────────  │
                     │  [ Context Manager ]    [ Memory System ]│
                     │  (Short-term Mem)       (JSON/Markdown)  │
                     └───────────┬────────────────────┬─────────┘
                                 │                    │
                    ┌────────────▼───────┐    ┌───────▼─────────────┐
                    │     LLM Client     │    │  MCP Tool Manager   │
                    │   (Reasoning Hub)  │    │   (Execution Hub)   │
                    └────────────────────┘    └───────┬─────────────┘
                                                      │
          ┌───────────────────────────────────────────┴──────────────────────────┐
          │                                    │                                 │
┌─────────▼──────────┐               ┌─────────▼──────────┐            ┌─────────▼──────────┐
│   Built-in Tools   │               │   Expert Skills    │            │    External MCP    │
│    (C++ Native)    │               │  (Modular Logic)   │            │   (Node / Python)  │
├────────────────────┤               ├────────────────────┤            ├────────────────────┤
│• Parallel Search   │               │• Code Architect    │            │• Puppeteer         │
│• AST Analysis      │               │• Debug Master      │            │• Google Search     │
│• Python Sandbox    │               │• Env Initializer   │            │• Custom Servers    │
└────────────────────┘               └────────────────────┘            └────────────────────┘
```

---

## 🧠 Core Concepts

### 🛠️ Tools: The "Digital Tentacles" (Actuation Layer)
> **Deep Perspective**: In the AI domain, perception and actuation are the only ways for an agent to interact with the physical world. Without Tools, AI is just a chat box; with Photon's Tools, it's a functional engineer.

*   **Holographic Perception**: Built-in high-performance parallel retrieval and AST analysis allow the AI to deconstruct large-scale project structures without wasting tokens.
*   **Atomic Intervention**: Ensures every execution is precise and safe through deterministic code injection (Diff Apply) and isolated sandboxes.
*   **Infinite Expansion**: Based on the **MCP (Model Context Protocol)**, supporting thousands of tool servers from the global developer community.

### 📜 Skills: The "Expert Instincts" (Expert Paradigms)
> **Deep Perspective**: General LLMs often lack engineering depth in specific domains. The Skills system bridges this gap by turning "experience" into "instinct."

*   **Logic Codification**: Modular `SKILL.md` files inject senior architects' thought processes (e.g., performance tuning, high-concurrency design) directly into the AI's decision layer.
*   **Proactive Evolution**: The AI proactively retrieves and reads relevant Skill guides before execution, ensuring outputs are industrial-grade engineering practices rather than simple probability predictions.

### 💾 Memory: "Neural Persistence" (Self-Continuity)
> **Deep Perspective**: AI without memory is just a single function call; AI with memory is an evolving life.

*   **Polymorphic Memory**: Combines JSON fragmented facts (for fast retrieval) with Markdown structured knowledge (for deep understanding).
*   **Habit Alignment**: Photon continuously records your project standards, historical bug lessons, and specific coding preferences. Over time, it moves from "collaboration" to "synchronization."

### 📑 Context: The "Cognitive Workspace" (High-Dimensional Thinking)
> **Deep Perspective**: LLM context windows are expensive and limited. The Context Manager is Photon's "Brain Butler."

*   **Smart Pruning & Compression**: When dialogue explodes, the built-in Context Manager identifies key decision points and uses the LLM for recursive summary compression.
*   **Lossless Reasoning**: Maintains core logic and historical decisions while significantly extending the AI's effective thinking length, enabling it to handle massive tasks involving tens of thousands of lines of code.

---

## 🚀 Quick Start

### 1. Download
Download the binary for your OS from the [Releases](https://github.com/QkHearn/Photon/releases) page.

### 2. Configuration
Create or edit `config.json` in the same directory as the program:

```json
{
  "llm": {
    "api_key": "YOUR_API_KEY",
    "base_url": "https://api.moonshot.cn/v1",
    "model": "kimi-k2-0905-preview"
  }
}
```

### 3. Launch
Run directly in your terminal, pointing to your project directory:

```bash
# Grant execution permission (Linux/macOS)
chmod +x photon

# Start analysis
./photon /path/to/your/project
```

### 4. LLM request adapters (Kimi, etc.)

Some providers (e.g. Kimi) return message formats they do not accept in the next request. Photon normalizes messages before every send so requests are valid. This is **design**, not a workaround.

- **`normalizeForKimi(messages)`:** Flatten assistant `content` from array to string; remove `name` from tool messages. See [docs/llm-adapters.md](docs/llm-adapters.md) for rationale and behavior.

---

<p align="center">
  <a href="README.md">English</a> | <a href="README.zh.md">中文</a> | <a href="README.jp.md">日本語</a>
</p>

<p align="center">
  <i>"Photon: The bridge between human intent and machine logic, at the speed of light."</i>
</p>
