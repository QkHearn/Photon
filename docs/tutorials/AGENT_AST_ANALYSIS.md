# 智能体 AST 分析功能

## 📋 概述

本文档说明 Photon 智能体如何主动进行 AST 分析，在 LLM 读取文件前提供符号摘要，从而减少 token 消耗并提高理解效率。

## 🎯 核心设计理念

### 传统方式的问题
```
LLM → 调用 read_file → 获得完整文件内容 (可能数千行)
    → 消耗大量 token
    → 难以快速定位关键符号
```

### 智能体增强方式
```
LLM → 计划调用 read_file
    ↓
Agent 拦截请求
    ↓
Agent 使用 TreeSitter/LSP 进行 AST 分析
    ↓
Agent 提取符号信息 (函数、类、结构体等)
    ↓
Agent 将符号摘要注入给 LLM
    ↓
LLM 查看符号摘要，选择需要的符号
    ↓
LLM 调用 view_symbol 获取特定符号的代码
```

## 🏗️ 实现架构

### 1. 拦截机制

在 `AgentRuntime::planPhase()` 中：

```cpp
// 提取工具调用
for (auto& toolCall : message["tool_calls"]) {
    // 🆕 拦截 read_file 请求,主动进行 AST 分析
    if (symbolMgr) {
        interceptAndAnalyzeFileRead(toolCall);
    }
    state.plannedActions.push_back(toolCall);
}
```

### 2. AST 分析

`AgentRuntime::interceptAndAnalyzeFileRead()` 实现：

```cpp
void AgentRuntime::interceptAndAnalyzeFileRead(nlohmann::json& toolCall) {
    // 1. 检测是否是 read_file 工具
    if (toolName != "read_file" && toolName != "read_code_file") return;
    
    // 2. 提取文件路径
    std::string filePath = extractFilePathFromArgs(args);
    
    // 3. 使用 SymbolManager 进行 AST 分析
    std::string symbolSummary = generateSymbolSummary(filePath);
    
    // 4. 将符号摘要注入为系统提示
    nlohmann::json hintMsg;
    hintMsg["role"] = "system";
    hintMsg["content"] = "📊 [Agent Analysis] File structure for `" + filePath + "`:\n\n" 
                        + symbolSummary 
                        + "\n\n💡 You can now ask to see specific symbols.";
    messageHistory.push_back(hintMsg);
}
```

### 3. 符号摘要生成

`AgentRuntime::generateSymbolSummary()` 返回格式化的符号列表：

```
### functions (5):
  - main - int main(int argc, char** argv) (lines 100-250) [tree-sitter]
  - parseConfig - Config parseConfig(string path) (lines 50-80) [lsp]
  - initLogger - void initLogger() (lines 30-45) [tree-sitter]
  ...

### classes (3):
  - LLMClient - class LLMClient (lines 300-500) [lsp]
  - AgentRuntime - class AgentRuntime (lines 600-800) [tree-sitter]
  ...
```

### 4. 查看特定符号

新增 `view_symbol` 工具，让 LLM 可以精确查看某个符号：

```json
{
  "name": "view_symbol",
  "description": "View the code of a specific symbol (function, class, method, etc.)",
  "parameters": {
    "file_path": "src/core/main.cpp",
    "symbol_name": "main"
  }
}
```

返回结果：
```
Symbol: main
Type: function
Location: src/core/main.cpp:100-250
Signature: int main(int argc, char** argv)
Source: tree-sitter

Code:
```cpp
int main(int argc, char** argv) {
    // ... 函数完整代码 ...
}
```
```

## 🔄 完整工作流程

### 场景：LLM 想了解 main.cpp 的结构

1. **LLM 发出请求**
   ```json
   {
     "tool": "read_file",
     "args": {"path": "src/core/main.cpp"}
   }
   ```

2. **Agent 拦截并分析**
   ```
   [Agent] 🔍 Intercepted file read: src/core/main.cpp
   [Agent] 🧠 Performing AST analysis...
   [Agent] ✅ Symbol summary injected (1234 chars)
   ```

3. **Agent 注入符号摘要**
   ```
   📊 [Agent Analysis] File structure for `src/core/main.cpp`:
   
   ### functions (12):
     - main (lines 100-250)
     - parseConfig (lines 50-80)
     - initLSP (lines 300-400)
     ...
   
   ### classes (3):
     - Config (lines 500-600)
     ...
   
   💡 You can now ask to see specific symbols instead of reading the entire file.
   ```

4. **LLM 查看符号摘要后选择**
   ```json
   {
     "tool": "view_symbol",
     "args": {
       "file_path": "src/core/main.cpp",
       "symbol_name": "main"
     }
   }
   ```

5. **Agent 返回精确的代码块**
   ```
   Symbol: main
   Lines: 100-250 (150 lines)
   Code: [只包含 main 函数的代码]
   ```

## 💡 优势

### Token 节省
- **传统方式**: 读取 1612 行文件 = ~8000 tokens
- **智能体方式**: 符号摘要 20 行 + 选中函数 150 行 = ~1000 tokens
- **节省率**: ~87.5%

### 理解效率
- LLM 先看到结构化的符号列表
- 可以快速定位感兴趣的函数/类
- 避免被无关代码干扰

### 精确定位
- 基于 AST 的精确行号
- 支持 Tree-sitter 和 LSP 双引擎
- 自动标注符号来源 (tree-sitter/lsp)

## 🛠️ 技术细节

### 符号提取引擎

1. **优先级**: LSP > Tree-sitter > Regex
2. **LSP**: 通过语言服务器获取最准确的符号信息
3. **Tree-sitter**: 本地快速 AST 解析
4. **Regex**: 兜底的简单模式匹配

### 支持的符号类型

- `function` - 函数
- `class` - 类
- `method` - 方法
- `struct` - 结构体
- `enum` - 枚举
- `interface` - 接口
- `variable` - 变量（类成员）

### 文件类型支持

- **C/C++**: `.cpp`, `.h`, `.hpp` (via clangd + tree-sitter)
- **Python**: `.py` (via pyright + tree-sitter)
- **TypeScript**: `.ts`, `.tsx` (via typescript-language-server + tree-sitter)
- **ArkTS**: `.ets` (via tree-sitter)

## 📝 配置

在 `config.json` 中启用：

```json
{
  "agent": {
    "enableTreeSitter": true,
    "symbolFallbackOnEmpty": true,
    "lspServers": [
      {
        "name": "Clangd",
        "command": "clangd",
        "extensions": [".cpp", ".h"]
      }
    ]
  }
}
```

## 🔧 开发者指南

### 添加新的符号提取器

1. 实现 `ISymbolProvider` 接口
2. 在 `SymbolManager::registerProvider()` 中注册
3. 优先级由注册顺序决定

### 扩展 view_symbol 工具

可以添加：
- 符号的调用图
- 符号的依赖关系
- 符号的文档注释提取

## 🎬 实际效果演示

### 用户请求
```
"请帮我理解 main.cpp 中的主要逻辑"
```

### Agent 行为
```
1. LLM 计划调用 read_file("main.cpp")
2. Agent 拦截，进行 AST 分析
3. Agent 注入符号摘要（12 个函数，3 个类）
4. LLM 看到摘要，选择查看 main() 函数
5. LLM 调用 view_symbol(main)
6. Agent 返回 main() 的 150 行代码
7. LLM 理解后给出解释
```

### Token 对比
- **不使用 Agent AST**: ~8000 tokens
- **使用 Agent AST**: ~1000 tokens
- **节省**: 7000 tokens (87.5%)

## 🚀 未来改进

1. **智能符号推荐**: Agent 根据用户意图主动推荐相关符号
2. **调用图分析**: 自动展示函数间的调用关系
3. **语义搜索**: 结合 SemanticManager 进行语义级别的符号匹配
4. **增量分析**: 只分析修改过的符号，提高性能

## 📚 相关文档

- [SymbolManager 设计](../README.md#symbolmanager)
- [Agent Runtime 架构](../REFACTOR_COMPLETE.md#agentruntime)
- [工具注册系统](../README.md#toolregistry)
