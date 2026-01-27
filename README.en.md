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

## 💎 Why Photon?

### ⚡ Native Velocity
*   **Hardcore Performance**: Built on pure C++17, achieving **microsecond-level response** for toolchain calls.
*   **Ultra-low Footprint**: Core memory usage is only **10MB~30MB**, starting 10x faster than interpreted languages.
*   **Zero-Dependency**: Single binary distribution with no Python/Node.js runtime requirements—true "plug-and-play" across all platforms.

### 🔌 Minimalist Integration
*   **Drop-in & Run**: No complex package installation; just download the binary and launch it in any terminal.
*   **Invisible Infrastructure**: Designed as an "invisible engine" that serves as the underlying reasoning and execution core for IDEs like Cursor and VS Code.
*   **CLI First**: A powerful command-line interface that brings "brains" to automation scripts and CI/CD pipelines.

### 🧠 Deep Engineering Perception
*   **Structural Cognition**: Built-in AST analysis goes beyond simple text matching to achieve deep understanding of complex code logic.
*   **Atomic Intervention**: Every code modification is deterministically verified and supports one-click `undo`, ensuring absolute safety of the engineering state.
*   **Expert Paradigm Injection**: Codifies senior architects' experience into AI native instincts via the modular Skills system.

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

---

<p align="center">
  <a href="README.md">中文</a> | <a href="README.en.md">English</a> | <a href="README.jp.md">日本語</a>
</p>

<p align="center">
  <i>"Photon: The bridge between human intent and machine logic, at the speed of light."</i>
</p>
