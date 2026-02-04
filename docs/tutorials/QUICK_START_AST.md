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

### 3. 观察工具行为
你会看到：
```
[ReadCodeBlock] Strategy: Generate symbol summary
```

### 4. LLM 看到的符号摘要
```
📊 Symbol Summary for: src/core/main.cpp

### functions (12):
  - `main` - int main(int argc, char** argv) (lines 100-250) [tree-sitter]
  - `parseConfig` (lines 50-80) [lsp]
  ...

💡 **Next Steps**:
  - Use `read_code_block` with `symbol_name` to view specific symbols
  - Use `read_code_block` with `start_line`/`end_line` for specific ranges
  - Use `semantic_search` to find code by concept or functionality
```

## 🎯 核心概念

### 传统方式
```
You: "读取 main.cpp"
LLM: [读取 1612 行，消耗 8000 tokens]
```

### 智能工具方式
```
You: "读取 main.cpp"
LLM: [调用 read_code_block("main.cpp")]
Tool: [检测到代码文件] → [返回符号摘要 20 行]
LLM: [看到摘要] → "我想看 main 函数"
LLM: [调用 read_code_block("main.cpp", symbol_name="main")]
Tool: [返回 main 函数的 150 行代码]
总 token: ~1000 (节省 87.5%)
```

## 🛠️ 可用工具

### `read_code_block` (智能模式)
智能读取文件内容,根据参数自动选择最佳策略。

**参数**:
- `file_path`: 文件路径 (必需)
- `symbol_name`: 符号名称 (可选) - 指定后只返回该符号的代码
- `start_line`: 起始行号 (可选) - 与 end_line 配合使用
- `end_line`: 结束行号 (可选) - 与 start_line 配合使用

**策略选择**:
1. 无额外参数 + 代码文件 → 返回符号摘要
2. 指定 `symbol_name` → 返回该符号的代码
3. 指定 `start_line`/`end_line` → 返回指定行范围
4. 其他情况 → 返回全文

**示例 1: 获取符号摘要**
```json
{
  "tool": "read_code_block",
  "args": {
    "file_path": "src/core/main.cpp"
  }
}
```

**示例 2: 读取特定符号**
```json
{
  "tool": "read_code_block",
  "args": {
    "file_path": "src/core/main.cpp",
    "symbol_name": "main"
  }
}
```

**示例 3: 读取行范围**
```json
{
  "tool": "read_code_block",
  "args": {
    "file_path": "src/core/main.cpp",
    "start_line": 100,
    "end_line": 250
  }
}
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

### 查看工具日志
```
[ReadCodeBlock] Strategy: Generate symbol summary
[ReadCodeBlock] Strategy: Read symbol 'main'
[ReadCodeBlock] Strategy: Read line range 100-250
[ReadCodeBlock] Strategy: Read full file
```

### 检查工具是否注册
```
> tools

[Core Tools]
- read_code_block     ← 智能模式,支持符号摘要
- semantic_search     ← 语义搜索
- apply_patch
- run_command
- list_project_files
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
4. 文件中没有符号 → 工具会自动降级到全文读取

**解决方法**:
- 检查日志中是否有 `[ReadCodeBlock] Strategy: Generate symbol summary`
- 如果看到 `Symbol summary failed, fallback to full file`,说明符号提取失败

### 问题: read_code_block 找不到符号
**可能原因**:
1. 符号名称错误 → 先不带参数调用获取符号摘要,查看准确名称
2. 文件路径错误 → 使用相对于项目根目录的路径
3. AST 分析失败 → 检查文件语法是否正确

**解决方法**:
- 工具会返回可用符号列表作为建议
- 检查符号名称是否与代码中完全一致(区分大小写)

### 问题: Token 没有节省
**可能原因**:
1. LLM 指定了行范围参数 → 工具会直接返回指定行,绕过符号摘要
2. 文件不是代码文件 → 工具会返回全文
3. 符号提取失败 → 工具会自动降级到全文读取

**解决方法**:
- 确保文件扩展名在支持列表中(`.cpp`, `.py`, `.ts` 等)
- 检查日志确认使用了哪种策略

## 📚 更多资源

- [完整技术文档](./AGENT_AST_ANALYSIS.md)
- [实现细节](../AGENT_AST_COMPLETE.md)
- [SymbolManager 文档](../README.md#symbolmanager)
- [Agent Runtime 架构](../REFACTOR_COMPLETE.md)
