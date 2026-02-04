# ReadCodeBlock 符号读取策略设计

## 背景

Photon 同时拥有多种符号提取方式：
1. **Tree-sitter AST** - 本地解析，快速可靠
2. **Regex 模式匹配** - 简单但有效的回退方案
3. **LSP (Language Server Protocol)** - 最精确，但依赖外部服务

本文档设计了一个智能的分层回退策略，充分利用各种方式的优势。

---

## 设计原则

### 1. 性能优先
- 优先使用已有索引（无 I/O 开销）
- 避免重复解析同一文件
- 异步预热常用文件

### 2. 可靠性保证
- 多层回退确保总能返回结果
- 不依赖单一数据源
- 优雅降级而非失败

### 3. 精确度平衡
- 索引数据：快但可能过时
- LSP 数据：精确但可能不可用
- 实时解析：最新但有性能开销

---

## 分层策略架构

```
┌─────────────────────────────────────────────────────────┐
│                    用户请求                              │
│  - 符号摘要 (无参数)                                     │
│  - 特定符号 (symbol_name)                               │
│  - 行范围 (start_line, end_line)                        │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 1: 本地符号索引 (SymbolManager)                   │
│ ✅ 速度：极快 (内存查询)                                 │
│ ✅ 可靠：不依赖外部服务                                  │
│ ⚠️  新鲜度：可能略有延迟 (5秒监视间隔)                   │
│                                                          │
│ 数据源：                                                 │
│  - Tree-sitter providers (C++, Python, TypeScript)      │
│  - Regex providers (通用回退)                           │
│  - 缓存在 .photon/index/symbols.json                    │
└─────────────────────────────────────────────────────────┘
                         ↓ (索引为空或未找到)
┌─────────────────────────────────────────────────────────┐
│ Layer 2: 实时 LSP 查询                                   │
│ ✅ 精确度：最高 (编译器级别)                             │
│ ✅ 新鲜度：实时                                          │
│ ⚠️  可用性：依赖 LSP 服务器状态                          │
│ ⚠️  性能：需要 IPC 通信                                  │
│                                                          │
│ 使用场景：                                               │
│  - 索引尚未构建完成                                      │
│  - 文件刚修改，索引未更新                                │
│  - 需要最精确的类型信息                                  │
└─────────────────────────────────────────────────────────┘
                         ↓ (LSP 不可用)
┌─────────────────────────────────────────────────────────┐
│ Layer 3: 临时 AST 解析 (On-Demand)                      │
│ ✅ 可靠：总是可用                                        │
│ ✅ 精确：基于 AST                                        │
│ ⚠️  性能：需要解析文件 (1-100ms)                         │
│                                                          │
│ 使用场景：                                               │
│  - 新创建的文件                                          │
│  - 索引被禁用                                            │
│  - 项目外的文件                                          │
└─────────────────────────────────────────────────────────┘
                         ↓ (解析失败)
┌─────────────────────────────────────────────────────────┐
│ Layer 4: 完整文件读取                                    │
│ ✅ 总是成功                                              │
│ ⚠️  无结构化信息                                         │
│                                                          │
│ 返回：原始文件内容 + 行号                                │
└─────────────────────────────────────────────────────────┘
```

---

## 实现细节

### 策略 1: 符号摘要生成

**目标：** 快速展示文件的结构概览

```cpp
nlohmann::json generateSymbolSummary(const std::string& filePath) {
    std::string normalizedPath = normalizePath(filePath);
    
    // Layer 1: 本地索引（最快）
    auto symbols = symbolMgr->getFileSymbols(normalizedPath);
    if (!symbols.empty()) {
        return formatSymbolSummary(symbols, "indexed", "tree-sitter/regex");
    }
    
    // Layer 2: LSP 实时查询（最精确）
    if (lspClient && lspClient->isReady()) {
        std::string fileUri = "file://" + getAbsolutePath(filePath);
        auto lspSymbols = lspClient->documentSymbols(fileUri);
        if (!lspSymbols.empty()) {
            auto converted = convertLSPSymbols(lspSymbols, normalizedPath);
            return formatSymbolSummary(converted, "lsp-realtime", "clangd");
        }
    }
    
    // Layer 3: 临时解析（按需）
    if (isCodeFile(filePath) && fileExists(filePath)) {
        auto tempSymbols = extractSymbolsOnDemand(filePath);
        if (!tempSymbols.empty()) {
            return formatSymbolSummary(tempSymbols, "on-demand", "tree-sitter");
        }
    }
    
    // Layer 4: 完整文件读取
    return readFullFile(filePath);
}
```

**输出示例：**

```json
{
  "content": [{
    "type": "text",
    "text": "📊 Symbol Summary for: src/agent/AgentRuntime.cpp\n\n### Classes (2):\n  - `AgentRuntime` (lines 18-676) [tree-sitter]\n  - `MemoryManager` (lines 13-16) [tree-sitter]\n\n### Functions (15):\n  - `executeTask` (lines 39-50) [tree-sitter]\n  - `handleToolCall` (lines 120-180) [tree-sitter]\n  ...\n\n💡 Data source: indexed (updated 2s ago)\n"
  }],
  "source": "indexed",
  "provider": "tree-sitter",
  "symbol_count": 17,
  "freshness": "2s ago"
}
```

---

### 策略 2: 读取特定符号

**目标：** 精确定位并读取某个符号的代码

```cpp
nlohmann::json readSymbolCode(const std::string& filePath, const std::string& symbolName) {
    std::string normalizedPath = normalizePath(filePath);
    
    // Layer 1: 索引查找
    auto symbols = symbolMgr->getFileSymbols(normalizedPath);
    auto* symbol = findSymbolByName(symbols, symbolName);
    if (symbol) {
        return readLineRange(filePath, symbol->line, symbol->endLine, 
                           "indexed", symbol->source);
    }
    
    // Layer 2: LSP 查找
    if (lspClient && lspClient->isReady()) {
        auto lspSymbol = lspClient->findSymbol(fileUri, symbolName);
        if (lspSymbol) {
            return readLineRange(filePath, lspSymbol.line, lspSymbol.endLine,
                               "lsp-realtime", "clangd");
        }
    }
    
    // Layer 3: 临时解析 + 模糊匹配
    if (isCodeFile(filePath)) {
        auto tempSymbols = extractSymbolsOnDemand(filePath);
        auto* fuzzyMatch = fuzzyFindSymbol(tempSymbols, symbolName, 0.8);
        if (fuzzyMatch) {
            return readLineRange(filePath, fuzzyMatch->line, fuzzyMatch->endLine,
                               "on-demand-fuzzy", "tree-sitter");
        }
    }
    
    // 失败：返回有用的错误信息
    return {
        {"error", "Symbol '" + symbolName + "' not found"},
        {"suggestion", listAvailableSymbols(filePath, 10)},
        {"hint", "Try using symbol summary (no parameters) to see all symbols"}
    };
}
```

---

### 策略 3: 智能缓存管理

```cpp
class SmartSymbolCache {
private:
    struct CacheEntry {
        std::vector<Symbol> symbols;
        std::time_t timestamp;
        std::string source;  // "indexed", "lsp", "on-demand"
    };
    
    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex cacheMutex;
    
public:
    // 获取符号（带缓存）
    std::vector<Symbol> getSymbols(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        
        auto it = cache.find(filePath);
        if (it != cache.end()) {
            auto age = std::time(nullptr) - it->second.timestamp;
            if (age < 5) {  // 5秒内的缓存有效
                return it->second.symbols;
            }
        }
        
        // 缓存失效，重新获取
        return refreshCache(filePath);
    }
    
    // 文件修改时失效缓存
    void invalidate(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        cache.erase(filePath);
    }
    
    // 后台预热
    void warmup(const std::vector<std::string>& frequentFiles) {
        for (const auto& file : frequentFiles) {
            std::thread([this, file]() {
                getSymbols(file);  // 触发缓存
            }).detach();
        }
    }
};
```

---

## 性能优化

### 1. 索引构建优化

```cpp
// 在 SymbolManager::performScan() 中
void SymbolManager::performScan() {
    // 优先级 1: 使用 Tree-sitter providers（快速且精确）
    for (const auto* provider : primaryProviders) {
        auto extracted = provider->extractSymbols(content, relPath);
        extractedAll.insert(extractedAll.end(), extracted.begin(), extracted.end());
    }
    
    // 优先级 2: LSP 作为回退（仅当 providers 失败时）
    if (extractedAll.empty() && lsp) {
        auto lspSymbols = lsp->documentSymbols(fileUri);
        extractedAll = convertLSPSymbols(lspSymbols);
    }
}
```

**原因：**
- Tree-sitter 不依赖外部服务，更可靠
- LSP 在批量扫描时可能未初始化
- 避免 LSP 返回空结果导致跳过 providers

### 2. 增量更新

```cpp
// 文件监视器检测到变化时
void onFileChanged(const std::string& filePath) {
    // 只更新单个文件，不重新扫描整个项目
    symbolManager.updateFile(filePath);
    
    // 同时通知 LSP
    if (lspClient) {
        lspClient->didChange(filePath, getFileContent(filePath));
    }
    
    // 失效相关缓存
    cache.invalidate(filePath);
}
```

### 3. 并行处理

```cpp
// 批量查询时并行处理
std::vector<nlohmann::json> readMultipleSymbols(
    const std::vector<std::pair<std::string, std::string>>& requests) {
    
    std::vector<std::future<nlohmann::json>> futures;
    
    for (const auto& [file, symbol] : requests) {
        futures.push_back(std::async(std::launch::async, [=]() {
            return readSymbolCode(file, symbol);
        }));
    }
    
    std::vector<nlohmann::json> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    return results;
}
```

---

## 错误处理

### 分层错误恢复

```cpp
nlohmann::json readWithErrorRecovery(const std::string& filePath) {
    try {
        // Layer 1: 索引
        return readFromIndex(filePath);
    } catch (const IndexNotReady& e) {
        std::cout << "[ReadCodeBlock] Index not ready, trying LSP..." << std::endl;
        
        try {
            // Layer 2: LSP
            return readFromLSP(filePath);
        } catch (const LSPNotAvailable& e) {
            std::cout << "[ReadCodeBlock] LSP not available, parsing on-demand..." << std::endl;
            
            try {
                // Layer 3: 临时解析
                return parseOnDemand(filePath);
            } catch (const ParseError& e) {
                std::cout << "[ReadCodeBlock] Parse failed, reading full file..." << std::endl;
                
                // Layer 4: 完整读取（总是成功）
                return readFullFile(filePath);
            }
        }
    }
}
```

---

## 配置选项

```json
{
  "symbol_strategy": {
    "prefer_lsp": false,           // 是否优先使用 LSP（默认 false）
    "enable_on_demand": true,      // 是否启用临时解析
    "cache_ttl_seconds": 5,        // 缓存有效期
    "warmup_frequent_files": true, // 是否预热常用文件
    "lsp_timeout_ms": 1000,        // LSP 查询超时
    "fallback_to_full_read": true  // 失败时是否回退到完整读取
  }
}
```

---

## 监控与调试

### 性能指标

```cpp
struct SymbolReadMetrics {
    int total_requests = 0;
    int index_hits = 0;      // 索引命中
    int lsp_hits = 0;        // LSP 命中
    int on_demand_hits = 0;  // 临时解析
    int full_read_hits = 0;  // 完整读取
    
    double avg_index_time_ms = 0;
    double avg_lsp_time_ms = 0;
    double avg_parse_time_ms = 0;
    
    void report() {
        std::cout << "=== Symbol Read Statistics ===" << std::endl;
        std::cout << "Total requests: " << total_requests << std::endl;
        std::cout << "Index hits: " << index_hits << " (" 
                  << (100.0 * index_hits / total_requests) << "%)" << std::endl;
        std::cout << "LSP hits: " << lsp_hits << std::endl;
        std::cout << "On-demand: " << on_demand_hits << std::endl;
        std::cout << "Full read: " << full_read_hits << std::endl;
        std::cout << "Avg index time: " << avg_index_time_ms << "ms" << std::endl;
        std::cout << "Avg LSP time: " << avg_lsp_time_ms << "ms" << std::endl;
    }
};
```

---

## 总结

### 优势

1. **性能优化**
   - 索引命中时 < 1ms
   - 避免重复解析
   - 智能缓存减少开销

2. **可靠性保证**
   - 4 层回退确保总能返回结果
   - 不依赖单一数据源
   - 优雅降级

3. **精确度平衡**
   - 索引：快速但可能略旧
   - LSP：精确但可能不可用
   - 临时解析：最新但有开销
   - 完整读取：总是可用

### 适用场景

| 场景 | 最佳策略 | 回退方案 |
|------|---------|---------|
| 快速浏览文件结构 | 索引 | LSP → 临时解析 |
| 精确跳转定义 | LSP | 索引 → 临时解析 |
| 新创建的文件 | 临时解析 | 完整读取 |
| 批量扫描项目 | 索引构建 | - |
| 外部文件查看 | 临时解析 | 完整读取 |

---

## 未来改进

1. **机器学习预测**
   - 预测用户接下来可能查看的文件
   - 提前预热缓存

2. **增量 AST**
   - 只重新解析修改的函数
   - 减少增量更新开销

3. **分布式索引**
   - 支持大型项目的分片索引
   - 并行构建加速

4. **语义增强**
   - 结合语义搜索理解代码意图
   - 智能推荐相关符号

---

## 相关文档

- `PATH_SEPARATOR_FIX.md` - 路径规范化修复
- `SEMANTIC_SEARCH_COMPLETE.md` - 语义搜索实现
- `QUICK_START_AST.md` - AST 工具使用指南
