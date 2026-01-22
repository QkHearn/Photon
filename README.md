# ⚛️ Photon

**极致轻量 · 高性能 · C++ 驱动的 AI 智能体**

Photon 是一个专为开发者设计的本地代码库助手。它基于 C++17 构建，旨在提供极速的文件分析能力与智能的上下文管理，同时通过 MCP 协议无缝扩展 AI 的能力边界。

---

## ✨ 核心特性

*   **⚡ 极速扫描**: 递归秒级检索本地源码，支持自定义后缀过滤。
*   **🧠 智能压缩**: 自动监控上下文长度，利用 LLM 摘要技术实时压缩对话历史。
*   **🔌 MCP 增强**: 原生支持 **Model Context Protocol**，可一键挂载 PDF 解析、Google 搜索、Python 沙箱等扩展。
*   **🎯 极致纯粹**: 无垃圾回收延迟，无重型运行时，仅需几 MB 内存即可高效运行。

---

## 🚀 快速开始

### 1. 准备依赖
- **macOS**: `brew install nlohmann-json openssl googletest`
- **Windows**: 安装 [vcpkg](https://github.com/microsoft/vcpkg) 并安装 `nlohmann-json`, `openssl`, `gtest`
- **Linux**: `sudo apt install libssl-dev libnlohmann-json-dev`

### 2. 构建与运行
在项目根目录下，我们提供了自动检测环境的构建脚本：

```bash
# macOS / Linux
./build.sh

# Windows (MSVC)
./build.bat
```

**启动程序:**
```bash
./photon <源码目录路径>
```

---

## ⚙️ 配置中心 (`config.json`)

Photon 的所有行为均可通过 JSON 进行微调，无需重新编译。

```json
{
    "llm": {
        "api_key": "sk-...",
        "base_url": "api.openai.com",
        "model": "gpt-4o-mini"
    },
    "agent": {
        "context_threshold": 5000,
        "file_extensions": [".cpp", ".h", ".py", ".md", ".json"]
    },
    "mcp_servers": [
        {
            "name": "doc-reader",
            "command": "npx -y @modelcontextprotocol/server-markitdown"
        }
    ]
}
```

---

## 🔌 MCP 服务器扩展

通过修改 `config.json` 中的 `mcp_servers` 数组，Photon 可以学会任何技能：

| 类型 | 示例命令 |
| :--- | :--- |
| **文档解析** | `npx -y @modelcontextprotocol/server-markitdown` |
| **Google 搜索** | `npx -y @modelcontextprotocol/server-google-search` |
| **Python 执行** | `uvx mcp-server-python` |
| **本地脚本** | `python3 ./my_tool.py` |

---

## 📂 模块划分

*   **`ContextManager`**: 负责对话大脑的上下文容量管理。
*   **`FileManager`**: 极速本地文件检索与内容读取。
*   **`MCPManager`**: 统一调度外部服务器工具流。
*   **`LLMClient`**: 封装 OpenAI 兼容协议的高性能客户端。

---

## 🧪 测试
```bash
# 执行单元测试
./agent_tests (或 ctest)
```

---
Photon —— 让 AI 以光速理解你的每一行代码。
# Photon
