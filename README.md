# <p align="center">⚛️ PHOTON: THE LOGIC ENGINE</p>

<p align="center">
  <img src="https://img.shields.io/badge/Status-Stable_v1.2-00FF00?style=flat-square" alt="Status">
  <img src="https://img.shields.io/badge/OS-Windows%20|%20macOS%20|%20Linux-E91E63?style=flat-square" alt="OS">
  <img src="https://img.shields.io/badge/Arch-C%2B%2B17_Static-00599C?style=flat-square&logo=c%2B%2B" alt="Engine">
  <img src="https://img.shields.io/badge/Safety-Photon_Guard-FFD600?style=flat-square" alt="Safety">
</p>

<p align="center">
  <b>「 逻辑已就绪。在纯粹的代码世界中，一切皆有定数。 」</b><br>
  <i>原生 C++ 驱动的超高性能 AI 编程代理 · 极速、确定、不可撼动</i>
</p>

---

## ⚡ 核心竞争力 [Core Competitiveness]

Photon 并非另一个臃肿的脚本集合，它是专为追求极致响应速度与工程确定性的开发者设计的 **原生逻辑中枢**。

*   **⚡ 降维打击级的搜索性能**:
    *   **Git-First Strategy**: 优先调用 Git 内核，毫秒级检索万级文件。
    *   **Parallel Pulse**: fallback 状态下自动启动多线程并行扫描，释放多核 CPU 潜能。
    *   **Binary Sniffer**: 自动嗅探并拦截二进制噪音，确保逻辑流的纯净。
*   **📦 绝对独立的单体部署**:
    *   基于 MSVC/Clang 深度静态链接，Windows 下无需任何 DLL 即可独立运行。
    *   **Tiny Footprint**: 几 MB 的体积，承载 17+ 种重型工程工具。
*   **🛡️ Photon Guard 协议**:
    *   内置高危指令拦截引擎，严禁路径逃逸与系统级破坏。
    *   **Shadow Backup**: 每次干涉代码前自动生成原子快照，支持 `undo` 时间回溯。
*   **🌐 跨平台大师**:
    *   针对 Windows 深度打磨，解决 Winsock 初始化、终端 UTF-8 乱码及 ANSI 全彩渲染。

---

## 🔬 军械库 [The Arsenal]

Photon 内置了 17 个高度集成的 MCP 模块，构成了完整的逻辑闭环：

| 模块 ID | 等级 | 技术效能 [Efficiency] |
| :--- | :--- | :--- |
| **file_search** | `Σ` | **极速脉冲**：Git 优先的全局文件定位，支持正则与内容感知。 |
| **grep_search** | `Ω` | **深层扫描**：带上下文（Snippet）的多线程符号追踪，直接透视源码。 |
| **diff_apply** | `Δ` | **精准干涉**：原子级 Search-and-Replace，最小化重构损耗。 |
| **python_sandbox** | `Π` | **逻辑演算**：受控环境下的 Python 实时执行，自动适配系统环境。 |
| **web_intelligence**| `Φ` | **全息检索**：集成 Markdown 化的 Web 抓取与专业社区（HarmonyOS）检索。 |
| **nexus_memory** | `μ` | **永恒记忆**：基于 `.photon/memory.json` 的长效跨会话知识锚定。 |

---

## 🛠️ 部署序列 [Deployment]

Photon 支持一键式物质化构建，产物为自包含的可执行文件。

### Windows (Self-Contained)
```powershell
# 使用 vcpkg 安装静态依赖并构建
cmake -B build -DCMAKE_TOOLCHAIN_FILE=... -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

### macOS / Linux
```bash
./build.sh
./photon <workspace_path>
```

---

## 🛡️ 安全防御 [Security Protocols]

Photon Guard 实时监控所有外部调用。任何包含以下特征的指令都将被立即掐断：
*   **路径逃逸**: 尝试使用 `../` 穿透至工作区外。
*   **提权倾向**: 包含 `sudo`、`runas` 或修改系统注册表。
*   **破坏指令**: `rm -rf /`、`format`、`del` 系统目录。

---

<p align="center">
  <i>"Reasoning through light, coding through logic. Photon is the final constant in your dev environment."</i>
</p>
