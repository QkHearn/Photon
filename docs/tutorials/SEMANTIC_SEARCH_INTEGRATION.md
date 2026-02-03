# 语义搜索集成文档

## 概述

语义搜索已作为 **Agent 内部能力** 集成到 Photon 中。这意味着当用户提出模糊查询时，Agent 会自动使用语义搜索来找到相关代码，而 LLM 不需要知道"语义搜索"这个工具的存在。

## 核心设计理念

### Agent 内部能力 vs LLM 工具

- **LLM 工具**: LLM 可以主动调用，列在工具列表中
- **Agent 内部能力**: Agent 在后台自动使用，对 LLM 透明

语义搜索采用内部能力方式，因为：
1. **简化 LLM 决策**: 不增加工具列表复杂度
2. **智能增强**: Agent 自主判断何时需要语义搜索
3. **无缝集成**: 搜索结果以自然语言形式注入上下文

## 工作流程

### 场景 1: 用户模糊查询

```
用户: "这个项目中处理用户登录逻辑的函数在哪里？"
    ↓
LLM: 尝试读取文件... → tool_call: read_code_block(path="登录逻辑")
    ↓
Agent 拦截:
  - 检测到路径参数包含中文/空格 → 识别为语义查询
  - 内部调用: semanticMgr->search("登录逻辑", 5)
  - 找到 Top-5 相关代码片段
    ↓
Agent 注入增强信息:
  system: "🔎 [Agent Semantic Search] Found 5 relevant code locations:
           
           **[1] src/auth/LoginHandler.cpp (lines 45-89)**
              Relevance: 92%
              Type: code
              Preview:
                class LoginHandler {
                  bool handleUserLogin(string user, string pass) {
                    ...
           
           **[2] src/api/AuthController.cpp (lines 120-150)**
              Relevance: 85%
              ...
           
           💡 Tip: Use read_code_block with the file paths above."
    ↓
LLM 再次规划:
  - 看到了相关文件和行号
  - 决定: read_code_block(path="src/auth/LoginHandler.cpp", start_line=45, end_line=89)
    ↓
返回给用户: "我找到了登录处理逻辑..."
```

### 场景 2: 检测查询意图的触发条件

Agent 在以下情况会触发语义搜索：

#### 触发条件 A: 工具参数包含模糊描述

当 `read_code_block` 或 `read_file` 的 `path` 参数满足：
- 包含空格 (如 "登录 逻辑")
- 包含中文字符
- 包含疑问词 (`where`, `what`, `how`, `哪`, `什么`, `如何`, `?`)

```cpp
// 示例触发
read_code_block(path="where is login logic")  // ✅ 触发
read_code_block(path="用户登录相关")           // ✅ 触发
read_code_block(path="src/auth/login.cpp")    // ❌ 不触发（正常路径）
```

#### 触发条件 B: list_project_files 带查询参数

如果 `list_project_files` 包含 `query` 或 `pattern` 参数：

```cpp
list_project_files(query="authentication code")  // ✅ 触发语义搜索
```

## 代码架构

### 1. 头文件修改 (`AgentRuntime.h`)

```cpp
class AgentRuntime {
public:
    AgentRuntime(
        // ... 现有参数 ...
        SemanticManager* semanticManager = nullptr  // ← 新增
    );

private:
    // ========== 语义搜索能力 ==========
    
    /**
     * @brief 拦截并增强模糊查询
     * 当检测到 LLM 的查询意图但不确定具体位置时,
     * Agent 主动使用语义搜索提供候选代码片段
     */
    void interceptAndEnhanceQuery(nlohmann::json& toolCall);
    
    /**
     * @brief 执行语义搜索 (内部能力)
     * @param query 自然语言查询
     * @param topK 返回前 K 个结果
     * @return 相关代码片段的格式化摘要
     */
    std::string performSemanticSearch(const std::string& query, int topK = 5);
    
    /**
     * @brief 检测用户消息是否包含语义查询意图
     */
    std::string detectSemanticQueryIntent(const std::string& content);
    
    SemanticManager* semanticMgr;  // ← 新增成员
};
```

### 2. 实现文件 (`AgentRuntime.cpp`)

#### 拦截逻辑

在 `planPhase()` 中添加拦截调用：

```cpp
void AgentRuntime::planPhase() {
    // ... LLM 生成工具调用 ...
    
    for (auto& toolCall : message["tool_calls"]) {
        // 现有能力: AST 分析
        if (symbolMgr) {
            interceptAndAnalyzeFileRead(toolCall);
        }
        
        // 新增能力: 语义搜索 ← 这里
        if (semanticMgr) {
            interceptAndEnhanceQuery(toolCall);
        }
        
        state.plannedActions.push_back(toolCall);
    }
}
```

#### 语义搜索实现

```cpp
void AgentRuntime::interceptAndEnhanceQuery(nlohmann::json& toolCall) {
    // 1. 检测工具类型和参数
    // 2. 判断是否为模糊查询
    // 3. 调用 performSemanticSearch()
    // 4. 注入搜索结果到消息历史
}

std::string AgentRuntime::performSemanticSearch(const std::string& query, int topK) {
    // 1. 调用 semanticMgr->search(query, topK)
    // 2. 格式化结果为 Markdown
    // 3. 返回格式化字符串
}
```

### 3. 主程序集成 (`main.cpp`)

当使用 AgentRuntime 时，需要传入 SemanticManager：

```cpp
// 创建 SemanticManager
auto semanticManager = std::make_shared<SemanticManager>(path, llmClient);
semanticManager->startAsyncIndexing();

// 创建 AgentRuntime（如果使用）
AgentRuntime agentRuntime(
    llmClient,
    toolRegistry,
    &symbolManager,
    nullptr,           // memoryManager
    &skillManager,
    semanticManager.get()  // ← 传入 SemanticManager
);

// 执行任务
agentRuntime.executeTask("用户查询...");
```

## 输出格式

语义搜索结果以系统消息形式注入：

```markdown
🔎 [Agent Semantic Search] Found 5 relevant code locations for query: "用户登录逻辑"

**[1] src/auth/LoginHandler.cpp (lines 45-89)**
   Relevance: 92.35%
   Type: code
   Preview:
     class LoginHandler {
       bool handleUserLogin(string username, string password) {
         // Validate credentials
     ...

**[2] src/api/AuthController.cpp (lines 120-150)**
   Relevance: 85.72%
   Type: code
   Preview:
     void AuthController::login(Request& req, Response& res) {
       auto username = req.body["username"];
     ...

💡 **Tip**: Use `read_code_block` with the file path and line numbers above to see the full code.
```

## 优势

### 1. 对 LLM 透明
- LLM 不需要学习新工具
- 工具列表保持简洁

### 2. 智能增强
- Agent 自主判断何时使用语义搜索
- 减少 LLM 的决策负担

### 3. 无缝集成
- 搜索结果以自然语言形式呈现
- LLM 可以直接理解和使用

### 4. 渐进式查询
```
用户查询 → 语义搜索（粗定位）→ 精确读取（细节查看）
```

## 未来扩展

### 1. 主动查询检测
在 `executeTask()` 初期分析用户消息，直接触发语义搜索：

```cpp
void AgentRuntime::executeTask(const std::string& userGoal) {
    // 检测查询意图
    std::string query = detectSemanticQueryIntent(userGoal);
    if (!query.empty() && semanticMgr) {
        // 主动进行语义搜索
        std::string results = performSemanticSearch(query);
        // 注入为系统消息
    }
    // ... 正常流程 ...
}
```

### 2. 多模态搜索
结合符号搜索和语义搜索：
- 先用符号搜索（快速，精确）
- 如果结果少，补充语义搜索（广泛，模糊）

### 3. 上下文感知
根据对话历史调整搜索策略：
- 如果用户多次询问同一主题，提高相关区域权重
- 记录用户已查看的代码，避免重复推荐

## 测试场景

### 场景 1: 功能定位
```
用户: "这个项目中处理 WebSocket 连接的代码在哪？"
预期: Agent 找到 WebSocket 相关代码片段
```

### 场景 2: 错误追踪
```
用户: "找到所有处理数据库连接错误的地方"
预期: Agent 找到错误处理相关代码
```

### 场景 3: API 查询
```
用户: "如何调用用户认证 API？"
预期: Agent 找到 API 使用示例
```

## 性能考虑

### 1. 异步索引
- `SemanticManager::startAsyncIndexing()` 在后台运行
- 不阻塞主程序启动

### 2. 索引持久化
- 支持 JSON 和 SQLite 两种存储方式
- 避免重复计算 embeddings

### 3. Top-K 限制
- 默认返回 Top-5 结果
- 可根据需要调整 `topK` 参数

## 配置选项

在 `config.json` 中可以配置（未来扩展）：

```json
{
  "agent": {
    "enableSemanticSearch": true,
    "semanticSearchTopK": 5,
    "semanticSearchThreshold": 0.7,
    "semanticIndexPath": ".photon/index/semantic_index.sqlite"
  }
}
```

## 总结

语义搜索作为 Agent 内部能力的集成方式，提供了：
1. **智能增强**: Agent 自主使用语义搜索辅助 LLM
2. **透明操作**: LLM 无感知，只看到增强后的上下文
3. **灵活扩展**: 可以轻松添加更多内部能力

这种设计模式可以作为其他高级功能（如依赖分析、影响范围分析等）的参考实现。
