# Tree-sitter 符号提取修复完成

## 问题描述

用户报告了两个问题：

1. **扫描完成日志总是显示在前台**
   ```
   [SymbolManager] Scan complete: 3964 files found, 245 scanned, 3719 ignored, 1864 symbols extracted
   ```

2. **所有符号的 source 都是 "regex"，Tree-sitter 没有被使用**

## 根本原因

### 问题 1：日志无条件打印
在 `SymbolManager.cpp` 第 137-139 行，扫描完成的日志是无条件打印的，不受 `PHOTON_DEBUG_SCAN` 环境变量控制。

### 问题 2：Tree-sitter 提取符号失败
Tree-sitter provider 虽然已经正确注册并被选为 primary provider，但 `collectSymbols` 函数无法找到符号的 identifier。

**具体原因**：
- C++ 的 `function_definition` 节点的直接子节点是：
  1. `primitive_type` (返回类型)
  2. `function_declarator` (函数声明器，**包含函数名**)
  3. `compound_statement` (函数体)
  
- 原代码只在直接子节点中查找 `identifier`，但函数名实际上在 `function_declarator` 的子节点中
- 因此 Tree-sitter 提取的符号数量为 0，触发了回退机制，使用 regex provider

## 解决方案

### 修复 1：条件化日志输出
修改 `SymbolManager.cpp`，只在首次扫描或调试模式下显示扫描摘要：

```cpp
// 只在调试模式或首次扫描时显示摘要
static bool firstScan = true;
if (enableDebugLog || firstScan) {
    std::cout << "[SymbolManager] Scan complete: " << fileCount << " files found, " 
              << scannedCount << " scanned, " << ignoredCount << " ignored, "
              << symbols.size() << " symbols extracted" << std::endl;
    firstScan = false;
}
```

### 修复 2：递归查找 identifier
修改 `TreeSitterSymbolProvider.cpp` 的 `collectSymbols` 函数，添加递归查找逻辑：

```cpp
auto findIdentifier = [&](TSNode parent) -> TSNode {
    // 递归查找第一个 identifier 节点
    std::function<TSNode(TSNode)> search = [&](TSNode n) -> TSNode {
        const char* nodeType = ts_node_type(n);
        if (std::strcmp(nodeType, "identifier") == 0 || 
            std::strcmp(nodeType, "name") == 0 || 
            std::strcmp(nodeType, "field_identifier") == 0 ||
            std::strcmp(nodeType, "type_identifier") == 0) {
            return n;
        }
        uint32_t count = ts_node_child_count(n);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode result = search(ts_node_child(n, i));
            if (!ts_node_is_null(result)) {
                return result;
            }
        }
        TSNode nullNode = {};
        return nullNode;
    };
    return search(parent);
};
```

## 验证结果

修复后，符号提取统计：

**修复前**：
```json
[
  {
    "source": "regex",
    "count": 1864
  }
]
```

**修复后**：
```json
[
  {
    "source": "tree_sitter",
    "count": 411
  }
]
```

### 符号质量对比

**Regex provider**：
- 没有 `endLine` 信息（值为 0）
- 符号识别不准确（例如将初始化列表识别为函数）
- 示例：
  ```json
  {
    "endLine": 0,
    "line": 25,
    "name": "llm",
    "source": "regex",
    "type": "function"
  }
  ```

**Tree-sitter provider**：
- 有准确的 `endLine` 信息
- 符号识别准确
- 示例：
  ```json
  {
    "endLine": 37,
    "line": 18,
    "name": "AgentRuntime",
    "source": "tree_sitter",
    "type": "function"
  }
  ```

## 影响的文件

1. `src/analysis/SymbolManager.cpp` - 修复日志输出和调试代码清理
2. `src/analysis/providers/TreeSitterSymbolProvider.cpp` - 修复 identifier 查找逻辑
3. `src/core/main.cpp` - 清理调试输出

## 测试

扫描完成输出：
```
[SymbolManager] Scan complete: 3971 files found, 244 scanned, 3727 ignored, 411 symbols extracted
```

- 3971 个文件被发现
- 244 个文件被扫描（C++、Python、TypeScript 等）
- 3727 个文件被忽略（node_modules、.git、build、third_party 等）
- 411 个符号被提取（全部来自 tree_sitter）

## 总结

通过修复 Tree-sitter 的符号提取逻辑，现在系统能够：
1. 正确使用 Tree-sitter 解析 C++、Python、TypeScript 等代码
2. 提取准确的符号信息，包括起始和结束行号
3. 在正常运行时保持简洁的日志输出
4. 在调试模式下提供详细的扫描信息

Tree-sitter 相比 regex 的优势：
- 更准确的符号识别
- 完整的位置信息（包括 endLine）
- 更好的语法理解
- 支持更多语言特性
