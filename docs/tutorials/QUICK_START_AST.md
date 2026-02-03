# 智能体 AST 分析 - 快速开始

## 🚀 快速体验

### 1. 启动 Photon
```bash
cd build
./photon
```

### 2. 尝试读取文件
```
> 帮我理解 src/core/main.cpp 的主要逻辑
```

### 3. 观察 Agent 行为
你会看到：
```
[Agent] 🔍 Intercepted file read: src/core/main.cpp
[Agent] 🧠 Performing AST analysis...
[Agent] ✅ Symbol summary injected (1234 chars)
```

### 4. LLM 看到的符号摘要
```
📊 [Agent Analysis] File structure for `src/core/main.cpp`:

### functions (12):
  - main (lines 100-250) [tree-sitter]
  - parseConfig (lines 50-80) [lsp]
  ...

💡 You can now ask to see specific symbols.
```

## 🎯 核心概念

### 传统方式
```
You: "读取 main.cpp"
LLM: [读取 1612 行，消耗 8000 tokens]
```

### 智能体方式
```
You: "读取 main.cpp"
Agent: [拦截] → [AST 分析] → [注入符号摘要 20 行]
LLM: [看到摘要] → "我想看 main 函数"
Agent: [返回 main 函数的 150 行代码]
总 token: ~1000 (节省 87.5%)
```

## 🛠️ 可用工具

### `view_symbol`
查看文件中特定符号的代码。

**参数**:
- `file_path`: 文件路径 (如 `"src/core/main.cpp"`)
- `symbol_name`: 符号名称 (如 `"main"`, `"LLMClient"`)

**示例**:
```json
{
  "tool": "view_symbol",
  "args": {
    "file_path": "src/core/main.cpp",
    "symbol_name": "main"
  }
}
```

**返回**:
```
Symbol: main
Type: function
Location: src/core/main.cpp:100-250
Signature: int main(int argc, char** argv)
Code: [函数完整代码]
```

## 📊 支持的语言

| 语言 | 扩展名 | 引擎 |
|------|--------|------|
| C/C++ | `.cpp`, `.h`, `.hpp` | LSP (clangd) + Tree-sitter |
| Python | `.py` | LSP (pyright) + Tree-sitter |
| TypeScript | `.ts`, `.tsx` | LSP (typescript-language-server) + Tree-sitter |
| ArkTS | `.ets` | Tree-sitter |

## ⚙️ 配置

在 `config.json` 中：

```json
{
  "agent": {
    "enableTreeSitter": true,
    "symbolFallbackOnEmpty": true,
    "lspServers": [
      {
        "name": "Clangd",
        "command": "clangd",
        "extensions": [".cpp", ".h", ".hpp"]
      },
      {
        "name": "Pyright",
        "command": "pyright-langserver --stdio",
        "extensions": [".py"]
      }
    ]
  }
}
```

## 🎬 使用场景

### 场景 1: 理解新项目
```
You: "帮我理解这个项目的入口文件"
Agent: [自动分析 main.cpp，提供符号摘要]
LLM: "这个项目有以下主要组件：LLMClient, SymbolManager, AgentRuntime..."
```

### 场景 2: 查找特定功能
```
You: "配置文件是如何加载的？"
LLM: [查看符号摘要] "我看到有 parseConfig 函数"
LLM: [调用 view_symbol] "让我看看这个函数"
Agent: [返回 parseConfig 的代码]
LLM: "配置加载流程是：1. 读取 JSON, 2. 解析参数, 3. 验证..."
```

### 场景 3: 重构代码
```
You: "帮我重构 LLMClient 类"
LLM: [查看符号摘要] "LLMClient 在 lines 500-600"
LLM: [调用 view_symbol("LLMClient")] 
Agent: [返回类的完整代码]
LLM: "建议将这个类拆分为..."
```

## 💡 最佳实践

### 1. 让 Agent 先分析
```
✅ "帮我理解 main.cpp"  (Agent 会自动分析)
❌ "读取 main.cpp 的全部内容"  (可能绕过 Agent)
```

### 2. 充分利用符号摘要
```
✅ LLM 看到摘要后: "我想看 main 函数"
❌ 直接要求: "读取 lines 100-250"
```

### 3. 分步骤理解
```
Step 1: 查看文件结构 (符号摘要)
Step 2: 选择关键符号
Step 3: 深入理解具体代码
```

## 🔍 调试

### 查看 Agent 日志
```
[Agent] Planning...
[Agent] 🔍 Intercepted file read: src/core/main.cpp
[Agent] 🧠 Performing AST analysis...
[Agent] ✅ Symbol summary injected (1234 chars)
```

### 检查工具是否注册
```
> tools

[Core Tools]
- read_code_block
- apply_patch
- run_command
- list_project_files
- view_symbol  ← 新工具
```

### 验证 AST 引擎
启动时检查：
```
✔ Tree-sitter enabled
✔ Registered 1 LSP servers
✔ Symbol scan completed (1234 symbols)
```

## 📈 性能对比

### 小文件 (~200 行)
- 传统: 1000 tokens
- 智能体: 300 tokens
- 节省: 70%

### 中等文件 (~1000 行)
- 传统: 5000 tokens
- 智能体: 800 tokens
- 节省: 84%

### 大文件 (~2000 行)
- 传统: 10000 tokens
- 智能体: 1200 tokens
- 节省: 88%

## 🆘 故障排除

### 问题: 没有看到符号摘要
**可能原因**:
1. Tree-sitter 未启用 → 检查 `config.json`
2. LSP 未运行 → 检查 `lspServers` 配置
3. 文件扩展名不支持 → 添加对应的 LSP 或 Tree-sitter 支持

### 问题: view_symbol 找不到符号
**可能原因**:
1. 符号名称错误 → 检查符号摘要中的准确名称
2. 文件路径错误 → 使用相对于项目根目录的路径
3. AST 分析失败 → 检查文件语法是否正确

### 问题: Token 没有节省
**可能原因**:
1. LLM 仍在调用 `read_file` 全文读取 → 引导 LLM 使用 `view_symbol`
2. Agent 拦截失败 → 检查日志是否有 "Intercepted" 消息

## 📚 更多资源

- [完整技术文档](./AGENT_AST_ANALYSIS.md)
- [实现细节](../AGENT_AST_COMPLETE.md)
- [SymbolManager 文档](../README.md#symbolmanager)
- [Agent Runtime 架构](../REFACTOR_COMPLETE.md)
