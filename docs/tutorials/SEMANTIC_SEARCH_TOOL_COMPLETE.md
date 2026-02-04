# 语义搜索工具化完成

## 🎯 目标

将语义搜索从 Agent 拦截模式改为独立工具,与符号分析工具保持一致的架构。

## 📊 对比分析

### 原有实现 (Agent 拦截模式)

**位置**: `AgentRuntime::interceptAndEnhanceQuery`

```cpp
// 在 planPhase 中拦截工具调用
if (toolName == "read_code_block") {
    // 启发式检测: 路径包含空格、中文、疑问词
    if (hasSpace || hasChinese || hasQuestion) {
        // 执行语义搜索
        std::string searchResults = performSemanticSearch(path);
        // 注入到消息历史
        messageHistory.push_back(hintMsg);
    }
}
```

**问题**:
1. ❌ 依赖启发式检测(不可靠)
2. ❌ LLM 无法主动调用
3. ❌ 拦截时机问题(与 AST 分析相同的问题)
4. ❌ 功能隐藏,用户不知道有语义搜索
5. ❌ 无法指定搜索参数(如 top_k)

### 新实现 (独立工具模式)

**位置**: `SemanticSearchTool`

```cpp
// LLM 主动调用
{
  "tool": "semantic_search",
  "args": {
    "query": "how is authentication handled?",
    "top_k": 5
  }
}
```

**优势**:
1. ✅ LLM 可以主动调用
2. ✅ 参数可配置(query, top_k)
3. ✅ 功能明确,易于发现
4. ✅ 与其他工具架构一致
5. ✅ 易于测试和维护

## 🛠️ 实现细节

### 1. 工具接口

**文件**: `src/tools/SemanticSearchTool.h`

```cpp
class SemanticSearchTool : public ITool {
public:
    explicit SemanticSearchTool(SemanticManager* semanticMgr);
    
    std::string getName() const override { return "semantic_search"; }
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    SemanticManager* semanticMgr;
    std::string formatSearchResults(const std::vector<SemanticChunk>& chunks, 
                                    const std::string& query);
};
```

### 2. Schema 定义

```json
{
  "type": "object",
  "properties": {
    "query": {
      "type": "string",
      "description": "Natural language query describing what you're looking for"
    },
    "top_k": {
      "type": "integer",
      "description": "Number of results to return (default: 5, max: 20)",
      "default": 5,
      "minimum": 1,
      "maximum": 20
    }
  },
  "required": ["query"]
}
```

### 3. 工具描述

```
Search the codebase using natural language queries. 
This tool finds relevant code snippets based on semantic similarity, 
not just keyword matching. 
Use this when you need to find code by concept, functionality, or behavior.
```

### 4. 结果格式

```
🔎 Semantic Search Results for: "how is authentication handled?"

Found 5 relevant code locations:

**[1] src/auth/AuthManager.cpp (lines 45-89)**
   Relevance: 92.3%
   Type: code
   Preview:
     class AuthManager {
     public:
         bool authenticate(const User& user) {
             return validateCredentials(user);
         }
     ...

**[2] src/middleware/AuthMiddleware.cpp (lines 12-34)**
   Relevance: 87.5%
   Type: code
   Preview:
     void AuthMiddleware::process(Request& req) {
         if (!authManager->isAuthenticated(req)) {
             throw UnauthorizedException();
         }
     ...

💡 **Next Steps**:
  - Use `read_code_block` with file path and line numbers to see full code
  - Use `view_symbol` to see specific functions or classes
  - Refine your query if results aren't relevant
```

## 📝 代码变更

### 1. 新增文件

- ✅ `src/tools/SemanticSearchTool.h` - 工具头文件
- ✅ `src/tools/SemanticSearchTool.cpp` - 工具实现
- ✅ 更新 `CMakeLists.txt` - 添加编译目标
- ✅ 更新 `src/core/main.cpp` - 注册工具

### 2. 工具注册

```cpp
// 在 main.cpp 中
#include "tools/SemanticSearchTool.h"

// 注册工具
toolRegistry.registerTool(std::make_unique<SemanticSearchTool>(semanticManager.get()));
```

### 3. Agent 简化 (可选)

现在可以移除或简化 `interceptAndEnhanceQuery`,因为 LLM 可以直接调用 `semantic_search` 工具。

## 🎬 使用示例

### 场景 1: 查找功能实现

```bash
User: "Where is user authentication handled in the codebase?"

LLM: [调用 semantic_search]
{
  "query": "user authentication handling",
  "top_k": 5
}

Tool: [返回 5 个相关代码位置]

LLM: "Based on the semantic search results, user authentication is primarily 
     handled in the following locations:
     1. AuthManager.cpp - Main authentication logic
     2. AuthMiddleware.cpp - Request authentication middleware
     ..."
```

### 场景 2: 概念搜索

```bash
User: "Show me code that deals with file I/O"

LLM: [调用 semantic_search]
{
  "query": "file input output operations",
  "top_k": 10
}

Tool: [返回 10 个相关代码位置]

LLM: "Here are the main areas dealing with file I/O:
     1. FileManager.cpp - Core file operations
     2. ConfigLoader.cpp - Configuration file reading
     ..."
```

### 场景 3: 行为查询

```bash
User: "How does the system handle errors?"

LLM: [调用 semantic_search]
{
  "query": "error handling exception management",
  "top_k": 8
}

Tool: [返回 8 个相关代码位置]
```

## 📊 与符号分析的对比

| 特性 | 符号分析 (`view_symbol`) | 语义搜索 (`semantic_search`) |
|------|------------------------|----------------------------|
| 搜索方式 | 精确符号名称 | 自然语言查询 |
| 使用场景 | 查看特定函数/类 | 查找功能/概念 |
| 返回内容 | 单个符号的完整代码 | 多个相关代码片段 |
| 依赖 | SymbolManager (AST) | SemanticManager (Embedding) |
| 性能 | 快速 (索引查询) | 较慢 (向量相似度计算) |
| 准确性 | 100% (精确匹配) | ~80-95% (语义相似) |

## 🎯 工具组合使用

### 典型工作流

```
1. 用户提问: "Where is authentication handled?"
   ↓
2. LLM 调用: semantic_search("authentication handling")
   ↓
3. 工具返回: 5 个相关代码位置
   ↓
4. LLM 分析结果,选择最相关的
   ↓
5. LLM 调用: read_code_block("src/auth/AuthManager.cpp")
   ↓
6. 工具返回: 符号摘要
   ↓
7. LLM 调用: read_code_block("src/auth/AuthManager.cpp", symbol_name="authenticate")
   ↓
8. 工具返回: authenticate 函数的完整代码
   ↓
9. LLM 回答: "Authentication is handled in AuthManager::authenticate()..."
```

## 🚀 未来增强

### 可选改进

1. **混合搜索**: 结合关键词和语义搜索
   ```cpp
   {
     "query": "authentication",
     "mode": "hybrid",  // semantic, keyword, hybrid
     "top_k": 10
   }
   ```

2. **文件过滤**: 限制搜索范围
   ```cpp
   {
     "query": "authentication",
     "file_pattern": "src/auth/**/*.cpp",
     "top_k": 5
   }
   ```

3. **类型过滤**: 只搜索特定类型
   ```cpp
   {
     "query": "authentication",
     "types": ["code", "markdown"],  // 排除 "fact", "skill"
     "top_k": 5
   }
   ```

4. **相关性阈值**: 过滤低相关度结果
   ```cpp
   {
     "query": "authentication",
     "min_score": 0.7,  // 只返回相关度 > 70% 的结果
     "top_k": 10
   }
   ```

## 🔄 迁移指南

### 从 Agent 拦截迁移到工具调用

**旧方式** (Agent 自动拦截):
```
User: "读取 'authentication 相关代码'"
Agent: [检测到中文] → [执行语义搜索] → [注入结果]
```

**新方式** (LLM 主动调用):
```
User: "Where is authentication handled?"
LLM: [理解意图] → [调用 semantic_search] → [分析结果] → [回答]
```

### System Prompt 更新

可以在 System Prompt 中提示 LLM 使用语义搜索:

```
Available tools:
- semantic_search: Use this to find code by concept or functionality
  Example: semantic_search("how authentication works")
  
- view_symbol: Use this to view specific functions or classes
  Example: view_symbol("AuthManager", "authenticate")
  
- read_code_block: Use this to read file contents
  Example: read_code_block("src/auth/AuthManager.cpp")
```

## ✅ 验证清单

- [x] 创建 `SemanticSearchTool` 类
- [x] 实现工具接口 (getName, getDescription, getSchema, execute)
- [x] 添加到 CMakeLists.txt
- [x] 在 main.cpp 中注册工具
- [x] 编译通过
- [x] 工具可以被 LLM 调用
- [x] 结果格式友好
- [x] 错误处理完善
- [x] 与其他工具架构一致

## 🎉 总结

成功将语义搜索从 Agent 拦截模式迁移到独立工具模式:

### 架构一致性 ✅

现在所有智能分析功能都是独立工具:

| 功能 | 工具名称 | 依赖 |
|------|---------|------|
| 读取文件 | `read_code_block` | 文件系统 |
| 符号摘要 | `read_code_block` (智能模式) | SymbolManager |
| 查看符号 | `view_symbol` | SymbolManager |
| 语义搜索 | `semantic_search` | SemanticManager |

### 设计原则 ✅

1. **工具自治**: 工具决定"如何"返回数据
2. **Agent 调度**: Agent 决定"何时"调用工具
3. **LLM 驱动**: LLM 根据需求选择合适的工具
4. **降级策略**: 工具不可用时有合理的降级方案

### 用户体验 ✅

1. **功能可见**: LLM 知道有语义搜索工具
2. **主动使用**: LLM 会在适当时候使用
3. **参数可控**: 用户可以通过 LLM 调整搜索参数
4. **结果清晰**: 格式化的搜索结果易于理解

**完全符合"工具智能化"的设计理念!** 🎊
