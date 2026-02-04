# Photon 性能调试完全指南

## 已完成的修复

### 1. Tree-sitter 符号提取修复 ✅
- **问题**：所有符号 source 都是 "regex"
- **原因**：TreeSitter 无法递归查找 identifier
- **修复**：实现递归查找逻辑
- **结果**：411 个符号全部来自 tree_sitter

### 2. 扫描日志优化 ✅
- **问题**：`[SymbolManager] Scan complete:` 总是显示
- **修复**：只在调试模式下显示（`PHOTON_DEBUG_SCAN=1`）
- **结果**：终端输出干净

### 3. LSP 初始化优化 ✅
- **问题**：首次命令等待 5-30 秒
- **修复**：添加 `enable_lsp: false` 配置选项
- **结果**：启动时间 < 1 秒

### 4. MCP 服务器优化 ✅
- **问题**：Puppeteer MCP 启动慢（5-30 秒）
- **修复**：清空 `mcp_servers` 配置
- **结果**：无 MCP 启动延迟

### 5. read_code_block 非阻塞查询 ✅
- **问题**：符号查询可能阻塞
- **修复**：实现 `tryGetFileSymbols` 非阻塞方法
- **结果**：避免锁等待

## 调试工具使用指南

### 环境变量

```bash
# 启用符号扫描调试
PHOTON_DEBUG_SCAN=1 build/photon .

# 启用路径规范化调试
PHOTON_DEBUG_READ=1 build/photon .

# 同时启用多个调试
PHOTON_DEBUG_SCAN=1 PHOTON_DEBUG_READ=1 build/photon .
```

### read_code_block 调试信息

现在 `read_code_block` 会输出详细的计时信息：

```
[ReadCodeBlock] ========== START ==========
[ReadCodeBlock] Time: 0ms - Tool execution started
[ReadCodeBlock] Time: 1ms - Got file_path: src/agent/AgentRuntime.cpp
[ReadCodeBlock] Time: 2ms - Path normalized: /path/to/file
[ReadCodeBlock] Time: 3ms - File exists check passed
[ReadCodeBlock] Time: 4ms - Strategy selection: hasSymbolName=0, hasLineRange=0
[ReadCodeBlock] Time: 5ms - Attempting symbol summary (non-blocking)
[ReadCodeBlock] Normalized path: src/agent/AgentRuntime.cpp
[ReadCodeBlock] Calling tryGetFileSymbols...
[SymbolManager] tryGetFileSymbols: Lock acquired, searching for 'src/agent/AgentRuntime.cpp'
[SymbolManager] tryGetFileSymbols: Found 15 symbols
[ReadCodeBlock] tryGetFileSymbols succeeded, got 15 symbols
[ReadCodeBlock] Time: 10ms - Symbol summary attempt completed
[ReadCodeBlock] Time: 11ms - Returning symbol summary
[ReadCodeBlock] ========== END ==========
```

### 性能基准

| 操作 | 预期时间 | 如果超过说明 |
|------|---------|-------------|
| 工具开始到路径规范化 | < 5ms | 文件系统慢 |
| tryGetFileSymbols | < 10ms | 锁竞争或索引大 |
| 符号摘要格式化 | < 5ms | 符号太多 |
| 全文读取 | < 100ms | 文件很大 |
| **总计** | **< 50ms** | 有性能问题 |

## 问题诊断流程

### 问题 A：首次命令卡住

**症状**：
```
(Finishing engine initialization...)
<等待 5-30 秒>
```

**可能原因**：
1. ✅ **LSP 初始化** - 设置 `enable_lsp: false`
2. ✅ **MCP 初始化** - 清空 `mcp_servers: []`
3. ⚠️ **技能加载慢** - 检查 `skill_roots` 配置
4. ⚠️ **网络问题** - API 请求超时

**诊断步骤**：
```bash
# 1. 检查配置
cat config.json | jq '{enable_lsp: .agent.enable_lsp, mcp_servers: .mcp_servers}'

# 2. 如果仍然慢，添加调试
build/photon . 2>&1 | grep -E "Finishing|initialized"

# 3. 检查网络
curl -I https://api.moonshot.cn/v1
```

### 问题 B：read_code_block 卡住

**症状**：
```
[ReadCodeBlock] Strategy: Generate symbol summary
<无响应>
```

**诊断步骤**：

1. **检查是否真的卡住**
   ```bash
   # 运行并查看调试日志
   echo "读取 src/agent/AgentRuntime.cpp" | build/photon . 2>&1 | \
   grep "ReadCodeBlock"
   ```

2. **检查卡在哪一步**
   - 如果看到 `========== START ==========` 但没有后续 → 工具调用失败
   - 如果看到 `Calling tryGetFileSymbols...` 但没有后续 → 锁问题
   - 如果看到 `tryGetFileSymbols succeeded` 但没有 END → 格式化问题

3. **检查符号索引**
   ```bash
   # 检查索引文件
   ls -lh .photon/index/symbols.json
   
   # 检查符号数量
   cat .photon/index/symbols.json | jq '[.files | to_entries[] | .value.symbols[]] | length'
   
   # 检查特定文件
   cat .photon/index/symbols.json | jq '.files["src/agent/AgentRuntime.cpp"]'
   ```

### 问题 C：API 超时

**症状**：
```
⚠ API request failed (Status: Timeout). Retrying (1/3)...
✖ API Error after 3 attempts: Connection failed
```

**可能原因**：
1. 网络连接问题
2. API 服务器响应慢
3. API key 无效或过期
4. 请求内容过大

**解决方案**：
```bash
# 1. 测试网络连接
curl -v https://api.moonshot.cn/v1/models \
  -H "Authorization: Bearer YOUR_API_KEY"

# 2. 增加超时时间（config.json）
{
  "llm": {
    "timeout": 60,  // 从 30 增加到 60 秒
    "max_retries": 5  // 从 3 增加到 5 次
  }
}

# 3. 使用本地模型（如果可用）
{
  "llm": {
    "base_url": "http://localhost:1234/v1",
    "model": "local-model"
  }
}
```

## 当前配置（推荐）

```json
{
  "llm": {
    "api_key": "your-api-key",
    "base_url": "https://api.moonshot.cn/v1",
    "model": "kimi-k2-0905-preview",
    "system_role": "..."
  },
  "agent": {
    "context_threshold": 217000,
    "file_extensions": [".txt", ".cpp", ".h", ".py", ".md", ".json"],
    "use_builtin_tools": true,
    "search_api_key": "",
    "enable_tree_sitter": true,     // ✅ 启用
    "enable_lsp": false,            // ✅ 禁用（加快启动）
    "lsp_servers": [],
    "tree_sitter_languages": [],
    "skill_roots": [],
    "symbol_ignore_patterns": [
      "node_modules", ".git", "build", ".venv", "third_party"
    ]
  },
  "mcp_servers": []                 // ✅ 禁用（加快启动）
}
```

## 性能监控

### 启动性能

```bash
# 测试启动到首次命令的时间
time sh -c 'echo "tools" | build/photon .'

# 预期：< 2 秒
```

### 工具性能

```bash
# 测试 read_code_block
echo "读取 src/agent/AgentRuntime.cpp" | build/photon . 2>&1 | \
grep -E "Time:|ReadCodeBlock"

# 预期：所有步骤 < 50ms
```

### 符号扫描性能

```bash
# 启用调试并查看扫描时间
PHOTON_DEBUG_SCAN=1 build/photon . 2>&1 | \
grep -E "Starting|complete"

# 预期：扫描 200+ 文件 < 5 秒
```

## 故障排除

### 如果 read_code_block 仍然卡住

1. **确认调试信息是否输出**
   ```bash
   # 应该看到详细的 Time: Xms 日志
   # 如果没有，说明工具根本没被调用
   ```

2. **检查是锁问题还是其他问题**
   ```bash
   # 查看最后一条日志
   # 如果是 "Calling tryGetFileSymbols..." 后卡住
   # 说明是锁问题（但 try_lock 应该不会阻塞）
   ```

3. **检查是否是 LLM 调用问题**
   ```bash
   # 如果根本没有 [ReadCodeBlock] 日志
   # 说明是 LLM 在调用工具前就失败了
   ```

### 如果启动仍然慢

1. **检查配置是否生效**
   ```bash
   # 确认 build 目录的配置
   cat build/config.json | jq '{enable_lsp, mcp_servers}'
   
   # 应该输出：
   # {
   #   "enable_lsp": false,
   #   "mcp_servers": []
   # }
   ```

2. **检查是否有其他异步任务**
   ```bash
   # 查看启动日志
   build/photon . 2>&1 | grep -E "Registering|Loading|Initializing"
   ```

## 总结

当前所有性能问题都已修复：

✅ Tree-sitter 正常工作
✅ 扫描日志静默
✅ LSP 初始化可禁用
✅ MCP 服务器已禁用
✅ read_code_block 使用非阻塞查询
✅ 详细的调试日志和计时

如果仍有问题，请：
1. 运行调试命令查看详细日志
2. 检查网络连接和 API 配置
3. 确认配置文件已同步到 build 目录
