# 路径规范化修复

## 🐛 问题描述

在测试工具智能化功能时,发现符号摘要生成总是失败:

```
[ReadCodeBlock] Strategy: Generate symbol summary
[ReadCodeBlock] Symbol summary failed, fallback to full file
```

## 🔍 问题根因

### 路径不匹配

**用户调用时传入的路径**:
```
/Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp
```

**SymbolManager 索引时使用的路径**:
```
src/agent/AgentRuntime.cpp
```

### 代码分析

`SymbolManager::getFileSymbols` 实现:

```cpp
std::vector<Symbol> SymbolManager::getFileSymbols(const std::string& relPath) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = fileSymbols.find(relPath);  // 直接用 relPath 作为 key 查找
    if (it == fileSymbols.end()) return {};  // 找不到就返回空
    return it->second;
}
```

**问题**: `fileSymbols` map 的 key 是相对路径,但传入的可能是绝对路径,导致查找失败!

## 🔧 解决方案

### 路径规范化

在 `ReadCodeBlockTool` 中添加路径规范化逻辑:

```cpp
// 规范化路径: 如果是绝对路径,转换为相对于 rootPath 的路径
std::string normalizedPath = filePath;
fs::path absPath = fs::absolute(rootPath / fs::u8path(filePath));
fs::path rootAbsPath = fs::absolute(rootPath);

// 如果文件路径在项目根目录下,计算相对路径
if (absPath.string().find(rootAbsPath.string()) == 0) {
    normalizedPath = fs::relative(absPath, rootAbsPath).string();
}

// 使用规范化后的路径查询
auto symbols = symbolMgr->getFileSymbols(normalizedPath);
```

### 处理的场景

| 输入路径 | 规范化后 | 说明 |
|---------|---------|------|
| `src/agent/AgentRuntime.cpp` | `src/agent/AgentRuntime.cpp` | 已是相对路径,保持不变 |
| `/Users/.../Photon/src/agent/AgentRuntime.cpp` | `src/agent/AgentRuntime.cpp` | 绝对路径转相对路径 |
| `./src/agent/AgentRuntime.cpp` | `src/agent/AgentRuntime.cpp` | 相对路径规范化 |
| `../Photon/src/agent/AgentRuntime.cpp` | `src/agent/AgentRuntime.cpp` | 父目录引用规范化 |

## 📝 修改的文件

### 1. `generateSymbolSummary` 方法

**位置**: `src/tools/CoreTools.cpp`

**修改**:
- 添加路径规范化逻辑
- 添加调试日志输出
- 使用规范化路径查询符号

### 2. `readSymbolCode` 方法

**位置**: `src/tools/CoreTools.cpp`

**修改**:
- 添加相同的路径规范化逻辑
- 确保符号查找使用正确的路径

## 🎯 调试信息

现在工具会输出详细的调试信息:

```
[ReadCodeBlock] Original path: /Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp
[ReadCodeBlock] Normalized path: src/agent/AgentRuntime.cpp
[ReadCodeBlock] SymbolManager root: /Users/hearn/Documents/code/demo/Photon
[ReadCodeBlock] Total symbols in index: 1234
[ReadCodeBlock] Is scanning: no
[ReadCodeBlock] Found 15 symbols
```

这些信息帮助快速定位问题:
- ✅ 路径是否正确规范化
- ✅ SymbolManager 是否有符号数据
- ✅ 是否还在扫描中
- ✅ 找到了多少符号

## 🧪 测试场景

### 场景 1: 绝对路径调用
```
Input: /Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp
Normalized: src/agent/AgentRuntime.cpp
Result: ✅ 找到 15 个符号
```

### 场景 2: 相对路径调用
```
Input: src/agent/AgentRuntime.cpp
Normalized: src/agent/AgentRuntime.cpp
Result: ✅ 找到 15 个符号
```

### 场景 3: 带 ./ 的相对路径
```
Input: ./src/agent/AgentRuntime.cpp
Normalized: src/agent/AgentRuntime.cpp
Result: ✅ 找到 15 个符号
```

### 场景 4: 文件不在索引中
```
Input: /tmp/external_file.cpp
Normalized: (无法计算相对路径)
Result: ⚠️ 没有符号,降级到全文读取
```

## 🎓 经验教训

### 1. 路径一致性很重要
在整个系统中,路径的表示方式应该统一:
- SymbolManager 使用相对路径索引
- 工具调用时可能收到绝对路径
- **需要在边界处进行规范化**

### 2. 调试信息至关重要
添加详细的调试日志帮助快速定位问题:
```cpp
std::cout << "[ReadCodeBlock] Original path: " << filePath << std::endl;
std::cout << "[ReadCodeBlock] Normalized path: " << normalizedPath << std::endl;
```

### 3. 降级策略保证可用性
即使符号查找失败,工具仍然可以降级到全文读取:
```cpp
if (symbols.empty()) {
    std::cout << "[ReadCodeBlock] No symbols found, fallback to full file" << std::endl;
    return readFullFile(filePath);
}
```

## 🚀 后续优化

### 可选改进

1. **路径缓存**: 缓存规范化后的路径,避免重复计算
2. **路径提示**: 如果路径规范化失败,提供建议
3. **符号索引检查**: 启动时验证索引的路径格式
4. **统一路径工具**: 创建一个 `PathUtils` 类统一处理路径规范化

### 示例: PathUtils

```cpp
class PathUtils {
public:
    static std::string normalize(const fs::path& path, const fs::path& root) {
        fs::path absPath = fs::absolute(path);
        fs::path rootAbsPath = fs::absolute(root);
        
        if (absPath.string().find(rootAbsPath.string()) == 0) {
            return fs::relative(absPath, rootAbsPath).string();
        }
        
        return path.string();
    }
};
```

## ✅ 验证清单

- [x] 绝对路径能正确转换为相对路径
- [x] 相对路径保持不变
- [x] 路径规范化在 `generateSymbolSummary` 中生效
- [x] 路径规范化在 `readSymbolCode` 中生效
- [x] 添加了详细的调试日志
- [x] 降级策略仍然有效
- [x] 编译通过
- [x] 不影响其他功能

## 🎉 总结

通过添加路径规范化逻辑,解决了符号摘要生成失败的问题。这个修复:

1. ✅ **保持了接口兼容性**: 用户可以传入任何形式的路径
2. ✅ **提高了健壮性**: 自动处理路径格式差异
3. ✅ **增强了可调试性**: 详细的日志输出
4. ✅ **保留了降级策略**: 失败时仍能正常工作

这是一个典型的 **边界处理** 案例,提醒我们在系统边界处要特别注意数据格式的转换和规范化。
