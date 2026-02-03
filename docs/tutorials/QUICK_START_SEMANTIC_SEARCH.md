# 快速开始：语义搜索功能

## 5 分钟上手

### 1. 核心概念

语义搜索已作为 **Agent 内部能力** 集成，这意味着：
- ✅ 用户提出模糊查询
- ✅ Agent 自动使用语义搜索
- ✅ LLM 收到增强后的结果
- ✅ 无需额外配置

### 2. 使用方法

#### 在现有代码中启用

```cpp
// Step 1: 创建 SemanticManager
auto semanticManager = std::make_shared<SemanticManager>(projectPath, llmClient);
semanticManager->startAsyncIndexing();

// Step 2: 创建 AgentRuntime 时传入
AgentRuntime agent(
    llmClient,
    toolRegistry,
    &symbolManager,
    nullptr,              // memoryManager
    nullptr,              // skillManager
    semanticManager.get() // ← 传入这里
);

// Step 3: 正常使用
agent.executeTask("找到处理用户登录的代码");
```

### 3. 工作原理

```
用户查询: "找到登录相关的代码"
    ↓
LLM 生成: read_code_block(path="登录代码")
    ↓
Agent 检测: "登录代码"不是有效路径 → 触发语义搜索
    ↓
SemanticManager: search("登录代码") → 返回 top-5 相关片段
    ↓
Agent 注入: 将搜索结果添加到消息历史
    ↓
LLM 再次决策: 看到真实的文件路径和行号
    ↓
正确读取: read_code_block(path="src/auth/login.cpp", start_line=45)
```

### 4. 触发条件

语义搜索在以下情况自动触发：

✅ **路径包含空格**
```cpp
read_code_block(path="登录 逻辑")
```

✅ **路径包含中文**
```cpp
read_code_block(path="用户认证代码")
```

✅ **路径包含疑问词**
```cpp
read_code_block(path="where is login logic")
read_code_block(path="什么函数处理登录")
```

❌ **正常路径不触发**
```cpp
read_code_block(path="src/auth/login.cpp")  // 不触发，直接读取
```

### 5. 示例输出

Agent 注入的搜索结果格式：

```markdown
🔎 [Agent Semantic Search] Found 5 relevant code locations for query: "登录逻辑"

**[1] src/auth/LoginHandler.cpp (lines 45-89)**
   Relevance: 92.35%
   Type: code
   Preview:
     class LoginHandler {
       bool handleUserLogin(string username, string password) {
         if (!validateCredentials(username, password)) {
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

### 6. 配置选项

默认配置已优化，无需修改。如需调整：

```cpp
// 修改返回结果数量
std::string results = agent.performSemanticSearch("query", 10); // 默认 5

// 访问底层 SemanticManager
auto chunks = semanticManager->search("query", 5);
```

### 7. 性能优化

#### 异步索引
```cpp
// 索引在后台线程运行，不阻塞主程序
semanticManager->startAsyncIndexing();
// 程序可以继续运行，索引完成后自动可用
```

#### 持久化存储
```cpp
// 索引自动保存到文件
.photon/index/semantic_index.json        // JSON 格式
.photon/index/semantic_index.sqlite      // SQLite 格式（可选）

// 下次启动时自动加载，无需重建
```

## 常见问题

### Q1: 语义搜索什么时候可用？

**A:** 在 `startAsyncIndexing()` 调用后，索引会在后台建立。通常几秒到几分钟（取决于项目大小）。索引完成前搜索可能返回空结果。

### Q2: 如何查看索引进度？

**A:** 目前索引在后台静默运行。可以通过日志或文件大小判断：
```bash
ls -lh .photon/index/
```

### Q3: 语义搜索会调用 LLM API 吗？

**A:** 会。在建立索引时，每个代码块需要调用一次 embedding API。索引完成后，搜索本身只需一次 embedding 调用（查询向量化）。

### Q4: 如何禁用语义搜索？

**A:** 不传入 `semanticManager` 参数即可：
```cpp
AgentRuntime agent(
    llmClient,
    toolRegistry,
    &symbolManager,
    nullptr,
    nullptr,
    nullptr  // ← 不传入 semanticManager
);
```

### Q5: 可以手动触发语义搜索吗？

**A:** 可以，用于测试：
```cpp
// 直接调用 SemanticManager
auto chunks = semanticManager->search("登录逻辑", 5);

// 或使用 AgentRuntime 的方法
std::string formatted = agent.performSemanticSearch("登录逻辑");
```

## 测试验证

### 快速测试

```bash
# 1. 编译项目
./build.sh

# 2. 运行示例
./build/semantic_search_example

# 3. 或运行主程序
./build/photon
```

### 验证步骤

1. **索引建立**
   ```
   ✅ 看到 "正在后台建立语义索引..." 消息
   ✅ .photon/index/ 目录下生成索引文件
   ```

2. **搜索触发**
   ```
   输入: "找到处理 HTTP 请求的代码"
   ✅ 看到 "[Agent] 🔍 Detected semantic query" 日志
   ✅ 看到 "[Agent] 🧠 Performing semantic search" 日志
   ✅ 看到 "[Agent] ✅ Semantic search results injected" 日志
   ```

3. **结果验证**
   ```
   ✅ LLM 响应中包含具体的文件路径
   ✅ 文件路径确实存在且相关
   ✅ 用户得到了准确的答案
   ```

## 下一步

- 📖 阅读完整文档：[docs/SEMANTIC_SEARCH_INTEGRATION.md](SEMANTIC_SEARCH_INTEGRATION.md)
- 🎯 查看完成报告：[SEMANTIC_SEARCH_COMPLETE.md](../SEMANTIC_SEARCH_COMPLETE.md)
- 💻 查看示例代码：[examples/semantic_search_example.cpp](../examples/semantic_search_example.cpp)

## 总结

语义搜索作为 Agent 内部能力：
- ✅ **零配置** - 传入参数即可启用
- ✅ **自动触发** - Agent 智能检测查询意图
- ✅ **透明操作** - LLM 无感知，只看到增强结果
- ✅ **高效准确** - 从模糊查询到精确代码定位

**现在开始使用吧！** 🚀
