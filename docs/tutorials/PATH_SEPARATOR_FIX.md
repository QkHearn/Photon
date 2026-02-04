# ReadCodeBlock 路径分隔符修复

## 问题描述

用户报告 `ReadCodeBlock` 工具无法正确生成符号摘要，总是回退到读取完整文件。日志显示：

```
[ReadCodeBlock] Total symbols in index: 0
[ReadCodeBlock] Is scanning: no
[ReadCodeBlock] No symbols in index
[ReadCodeBlock] File exists but not in index
[ReadCodeBlock] Falling back to full file read
```

## 根本原因

经过分析发现了**两个问题**：

### 1. 路径分隔符不一致

**问题**：
- `SymbolManager` 在存储文件路径时使用 `generic_string()`，将路径分隔符统一为 `/`
- `ReadCodeBlockTool` 在规范化路径时使用 `string()`，在 Windows 上可能返回 `\` 分隔符
- 导致路径匹配失败，无法从索引中找到符号

**位置**：
- `src/analysis/SymbolManager.cpp:220` - 使用 `generic_string()`
- `src/tools/CoreTools.cpp:275` - 原来使用 `string()`

### 2. 符号扫描延迟初始化

**问题**：
- `SymbolManager` 在第 610 行被创建，但索引为空
- `symbolManager.startAsyncScan()` 只在第一次用户输入后才被调用（第 973 行）
- 在首次输入前使用 `ReadCodeBlock` 会发现索引为空

**位置**：
- `src/core/main.cpp:610` - SymbolManager 初始化
- `src/core/main.cpp:973` - 延迟启动扫描（在 `!engineInitialized` 块内）

## 修复方案

### 修复 1: 规范化 rootPath 的绝对路径

**问题**：当 `rootPath` 是 `.` 时，`fs::absolute(rootPath)` 会返回 `/path/to/project/.`，末尾的 `.` 导致路径比较失败。

**解决方案**：使用 `fs::canonical()` 来获取规范化的绝对路径（解析 `.` 和 `..` 等）：

```cpp
// 修改前
fs::path rootAbsPath = fs::absolute(rootPath);

// 修改后
fs::path rootAbsPath;
try {
    rootAbsPath = fs::canonical(rootPath);  // 规范化路径
} catch (...) {
    rootAbsPath = fs::absolute(rootPath);   // 回退方案
}
```

### 修复 2: 使用 lexically_relative 计算相对路径

**问题**：使用字符串匹配 `find()` 来判断路径关系不够可靠。

**解决方案**：使用 `lexically_relative()` 来计算相对路径，并检查结果是否有效：

```cpp
// 修改前
if (absPath.string().find(rootAbsPath.string()) == 0) {
    normalizedPath = fs::relative(absPath, rootAbsPath).string();
}

// 修改后
auto relPath = absPath.lexically_relative(rootAbsPath);
if (!relPath.empty() && relPath.string() != ".." && relPath.string().find("..") != 0) {
    normalizedPath = relPath.generic_string();  // 使用 generic_string() 统一分隔符
}
```

### 修复 3: 统一路径分隔符

在所有路径转换中使用 `.generic_string()` 而不是 `.string()`，确保路径分隔符统一为 `/`（与 SymbolManager 一致）。

### 修复 2: 增强调试信息

添加了详细的路径转换调试信息，帮助诊断路径问题：

```cpp
std::cout << "[ReadCodeBlock] === Path Normalization Debug ===" << std::endl;
std::cout << "[ReadCodeBlock] Original path: " << filePath << std::endl;
std::cout << "[ReadCodeBlock] SymbolManager root: " << symbolMgr->getRootPath() << std::endl;
std::cout << "[ReadCodeBlock] Root absolute path: " << rootAbsPath.string() << std::endl;
std::cout << "[ReadCodeBlock] File absolute path: " << absPath.string() << std::endl;
std::cout << "[ReadCodeBlock] Is input absolute? " << (inputPath.is_absolute() ? "yes" : "no") << std::endl;
```

## 测试建议

要验证修复是否有效：

1. **启动 Photon**
   ```bash
   ./photon
   ```

2. **输入任意命令**（触发引擎初始化和符号扫描）
   ```
   help
   ```

3. **等待几秒**，让符号索引构建完成

4. **使用 ReadCodeBlock 工具**，检查是否能正确生成符号摘要

5. **查看调试输出**，确认：
   - 路径正确转换为相对路径
   - 符号索引不为空
   - 成功找到文件符号

## 预期结果

修复后，`ReadCodeBlock` 应该能够：

1. ✅ 正确规范化绝对路径和相对路径
2. ✅ 在符号索引中找到文件
3. ✅ 生成符号摘要（而不是总是回退到读取完整文件）
4. ✅ 在 Windows 和 Unix 系统上都能正常工作

## 未来改进建议

### 建议 1: 提前启动符号扫描

将 `symbolManager.startAsyncScan()` 移到初始化阶段（第 630 行附近），而不是等到第一次用户输入：

```cpp
// 在 src/core/main.cpp:630 附近添加
symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
symbolManager.startAsyncScan();  // 提前启动扫描
symbolManager.startWatching(5);  // 提前启动监视
semanticManager->startAsyncIndexing();
```

这样用户第一次使用 `ReadCodeBlock` 时就能享受到符号摘要功能。

### 建议 2: 实现临时符号提取

在 `generateSymbolSummary()` 中，当文件不在索引中时，可以实时提取符号：

```cpp
// TODO: 未来可以实现临时符号提取
// symbols = extractSymbolsOnDemand(actualPath);
```

## 相关文件

- `src/tools/CoreTools.cpp` - ReadCodeBlock 工具实现
- `src/tools/CoreTools.h` - ReadCodeBlock 工具头文件
- `src/analysis/SymbolManager.cpp` - 符号管理器实现
- `src/analysis/SymbolManager.h` - 符号管理器头文件
- `src/core/main.cpp` - 主程序和初始化逻辑

## 修复日期

2026-02-04

## 修复作者

Cursor AI Assistant
