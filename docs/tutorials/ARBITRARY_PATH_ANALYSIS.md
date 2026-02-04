# 任意路径文件分析支持

## 📋 需求

用户希望能够分析 **任意路径** 的文件,包括:
- 项目内的文件 ✅
- 项目外的文件 ⚠️ (部分支持)
- 临时文件 ⚠️ (部分支持)
- 系统文件 ⚠️ (部分支持)

## 🎯 当前能力

### ✅ 完全支持: 项目内文件

**场景**: 分析项目内的任何代码文件

```bash
# 相对路径
read_code_block("src/agent/AgentRuntime.cpp")

# 绝对路径
read_code_block("/Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp")
```

**工作流程**:
```
1. 路径规范化 → src/agent/AgentRuntime.cpp
2. 查询符号索引 → 找到 15 个符号
3. 生成符号摘要 → 返回摘要
```

**优势**:
- ⚡ 快速(使用预构建的索引)
- 📊 完整(包含所有符号信息)
- 🎯 准确(LSP + Tree-sitter 双引擎)

### ⚠️ 部分支持: 项目外文件

**场景**: 分析项目外的代码文件

```bash
read_code_block("/tmp/test.cpp")
read_code_block("/Users/other/project/file.py")
```

**当前行为**:
```
1. 路径规范化 → 无法规范化(不在项目内)
2. 查询符号索引 → 未找到
3. 降级策略 → 返回全文内容
```

**限制**:
- ❌ 无符号摘要
- ❌ 无法使用 symbol_name 参数
- ✅ 但可以正常读取文件内容

## 🚀 改进方案

### 方案 1: 实时符号提取 (推荐) ⭐⭐⭐⭐⭐

**设计思路**: 对于项目外的文件,临时进行 AST 分析

#### 实现步骤

1. **检测文件是否在索引中**
   ```cpp
   auto symbols = symbolMgr->getFileSymbols(normalizedPath);
   if (symbols.empty()) {
       // 文件不在索引中
   }
   ```

2. **判断是否可以实时分析**
   ```cpp
   fs::path actualPath = findActualPath(filePath);
   if (fs::exists(actualPath) && isCodeFile(actualPath)) {
       // 可以实时分析
   }
   ```

3. **临时提取符号**
   ```cpp
   std::vector<Symbol> symbols = extractSymbolsOnDemand(actualPath);
   ```

4. **生成符号摘要**
   ```cpp
   return formatSymbolSummary(symbols, filePath);
   ```

#### 代码实现

```cpp
std::vector<Symbol> ReadCodeBlockTool::extractSymbolsOnDemand(const fs::path& filePath) {
    std::vector<Symbol> symbols;
    
    // 读取文件内容
    std::ifstream file(filePath);
    if (!file.is_open()) return symbols;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // 根据文件扩展名选择合适的 provider
    std::string ext = filePath.extension().string();
    
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
        // 使用 Tree-sitter C++ parser
        TreeSitterSymbolProvider provider;
        symbols = provider.extractSymbols(content, filePath.string());
    } else if (ext == ".py") {
        // 使用 Tree-sitter Python parser
        TreeSitterSymbolProvider provider;
        symbols = provider.extractSymbols(content, filePath.string());
    } else {
        // 使用 Regex fallback
        RegexSymbolProvider provider;
        symbols = provider.extractSymbols(content, filePath.string());
    }
    
    return symbols;
}
```

#### 优势
- ✅ 支持任意路径
- ✅ 不需要预先索引
- ✅ 保持工具接口一致
- ⚠️ 首次分析较慢(需要解析文件)

#### 劣势
- ⚠️ 每次都需要重新解析
- ⚠️ 无法利用缓存
- ⚠️ 无 LSP 支持(只能用 Tree-sitter)

### 方案 2: 扩展 SymbolManager 支持临时索引

**设计思路**: 让 SymbolManager 支持临时添加项目外文件

#### 实现步骤

1. **添加临时索引 API**
   ```cpp
   class SymbolManager {
   public:
       // 临时索引一个文件(不持久化)
       void indexTemporaryFile(const std::string& absolutePath);
       
       // 清理临时索引
       void clearTemporaryFiles();
   };
   ```

2. **在工具中使用**
   ```cpp
   if (symbols.empty() && fileOutsideProject) {
       symbolMgr->indexTemporaryFile(absolutePath);
       symbols = symbolMgr->getFileSymbols(absolutePath);
   }
   ```

#### 优势
- ✅ 可以利用 LSP 支持
- ✅ 统一的符号管理
- ✅ 可以缓存结果

#### 劣势
- ⚠️ 需要修改 SymbolManager 核心逻辑
- ⚠️ 临时文件管理复杂
- ⚠️ 可能影响性能

### 方案 3: 混合策略 (最佳) ⭐⭐⭐⭐⭐

**设计思路**: 结合方案 1 和方案 2 的优点

```
项目内文件 → 使用预构建索引 (快速)
    ↓
项目外文件 → 检查是否已临时索引
    ↓
未索引 → 实时提取符号 (慢但可用)
    ↓
缓存结果 → 下次访问更快
```

#### 实现

```cpp
nlohmann::json ReadCodeBlockTool::generateSymbolSummary(const std::string& filePath) {
    // 1. 尝试从索引获取
    auto symbols = symbolMgr->getFileSymbols(normalizedPath);
    
    if (!symbols.empty()) {
        std::cout << "[ReadCodeBlock] Using indexed symbols" << std::endl;
        return formatSymbolSummary(symbols, filePath);
    }
    
    // 2. 检查文件是否存在
    fs::path actualPath = findActualPath(filePath);
    if (!fs::exists(actualPath) || !isCodeFile(actualPath)) {
        result["error"] = "File not found or not a code file";
        return result;
    }
    
    // 3. 实时提取符号
    std::cout << "[ReadCodeBlock] File not in index, extracting symbols on-demand" << std::endl;
    symbols = extractSymbolsOnDemand(actualPath);
    
    if (symbols.empty()) {
        result["error"] = "Failed to extract symbols";
        return result;
    }
    
    // 4. 可选: 缓存到临时索引
    // symbolMgr->cacheTemporarySymbols(actualPath, symbols);
    
    return formatSymbolSummary(symbols, filePath);
}
```

## 📊 性能对比

| 场景 | 方案 | 首次访问 | 后续访问 | 内存占用 |
|------|------|---------|---------|---------|
| 项目内文件 | 预构建索引 | ~1ms | ~1ms | 中 |
| 项目外文件 (方案1) | 实时提取 | ~50ms | ~50ms | 低 |
| 项目外文件 (方案2) | 临时索引 | ~50ms | ~1ms | 高 |
| 项目外文件 (方案3) | 混合策略 | ~50ms | ~1ms | 中 |

## 🎯 推荐实现路径

### 阶段 1: 基础支持 (当前)
- ✅ 项目内文件完全支持
- ✅ 项目外文件降级到全文读取
- ✅ 路径规范化

### 阶段 2: 实时符号提取 (下一步)
- [ ] 实现 `extractSymbolsOnDemand`
- [ ] 支持主要语言 (C++, Python, TypeScript)
- [ ] 添加错误处理和降级策略

### 阶段 3: 智能缓存 (优化)
- [ ] 添加 LRU 缓存
- [ ] 支持文件变更检测
- [ ] 自动清理过期缓存

### 阶段 4: 完整支持 (未来)
- [ ] 扩展 SymbolManager 支持临时文件
- [ ] 支持更多语言
- [ ] 性能优化

## 💡 使用建议

### 当前版本

**推荐做法**:
```bash
# 项目内文件 - 直接使用
read_code_block("src/agent/AgentRuntime.cpp")

# 项目外文件 - 使用行范围读取
read_code_block("/tmp/test.cpp", start_line=1, end_line=50)
```

**不推荐**:
```bash
# 项目外大文件 - 会返回全文,消耗大量 token
read_code_block("/tmp/large_file.cpp")  # 可能 10000+ 行
```

### 未来版本 (实现方案 3 后)

**可以自由使用**:
```bash
# 任意路径都支持符号摘要
read_code_block("/tmp/test.cpp")
read_code_block("/Users/other/project/file.py")
read_code_block("~/Documents/code.cpp")
```

## 🔧 快速实现 (方案 1)

如果你现在就需要这个功能,可以快速实现方案 1:

```cpp
// 在 CoreTools.cpp 中添加
std::vector<Symbol> ReadCodeBlockTool::extractSymbolsOnDemand(const fs::path& filePath) {
    std::vector<Symbol> symbols;
    
    // 简单实现: 使用 regex 提取函数和类
    std::ifstream file(filePath);
    if (!file.is_open()) return symbols;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // C++ 函数正则
    std::regex funcRegex(R"(\b(\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    std::regex classRegex(R"(\bclass\s+(\w+))");
    
    std::sregex_iterator iter(content.begin(), content.end(), funcRegex);
    std::sregex_iterator end;
    
    int lineNum = 1;
    for (; iter != end; ++iter) {
        Symbol sym;
        sym.name = (*iter)[2].str();
        sym.type = "function";
        sym.source = "regex";
        sym.path = filePath.string();
        sym.line = lineNum;  // 简化: 需要计算实际行号
        symbols.push_back(sym);
    }
    
    return symbols;
}
```

然后在 `generateSymbolSummary` 中使用:

```cpp
if (symbols.empty() && fs::exists(actualPath)) {
    symbols = extractSymbolsOnDemand(actualPath);
}
```

## ✅ 总结

| 功能 | 当前状态 | 未来支持 |
|------|---------|---------|
| 项目内文件符号摘要 | ✅ 完全支持 | ✅ |
| 项目内文件符号读取 | ✅ 完全支持 | ✅ |
| 项目外文件全文读取 | ✅ 完全支持 | ✅ |
| 项目外文件符号摘要 | ❌ 不支持 | ⚠️ 可实现 |
| 项目外文件符号读取 | ❌ 不支持 | ⚠️ 可实现 |
| 临时文件分析 | ❌ 不支持 | ⚠️ 可实现 |

**结论**: 
- 当前可以 **读取** 任意路径的文件 ✅
- 但只能对 **项目内** 文件生成符号摘要 ⚠️
- 实现方案 1 或方案 3 可以支持任意路径的符号分析 🚀
