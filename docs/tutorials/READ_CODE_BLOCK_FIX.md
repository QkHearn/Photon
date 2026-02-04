# ReadCodeBlock 工具卡顿修复

## 问题描述

用户使用 `read_code_block` 工具读取代码文件时，会卡在 "Generate symbol summary" 步骤：

```
⚙️ [Action] core::read_code_block {"file_path":"/Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp"}
[ReadCodeBlock] Strategy: Generate symbol summary
<卡住，无响应>
```

## 根本原因

### 问题分析

1. **符号扫描是异步的**
   - 启动时，`SymbolManager` 会在后台线程扫描所有代码文件
   - 扫描过程可能需要几秒到几十秒（取决于项目大小）

2. **互斥锁竞争**
   - `SymbolManager::getFileSymbols()` 使用 `std::lock_guard<std::mutex>` 保护数据
   - 扫描线程在处理文件时会长时间持有这个锁
   - 当 `read_code_block` 尝试获取符号时，会被阻塞等待锁释放

3. **用户体验差**
   - 用户看到 "Generate symbol summary" 后就卡住
   - 没有进度提示或超时机制
   - 实际上是在等待扫描线程释放锁

### 代码路径

```
read_code_block
  └─> generateSymbolSummary()
       └─> symbolMgr->getFileSymbols()  // 尝试获取锁
            └─> std::lock_guard<std::mutex> lock(mtx);  // 阻塞在这里！
```

同时，扫描线程：
```
performScan()
  └─> scanFile()
       └─> std::lock_guard<std::mutex> lock(mtx);  // 持有锁
            └─> [处理文件，可能需要几百毫秒]
```

## 解决方案

### 修复策略

在尝试生成符号摘要之前，先检查扫描是否完成：

```cpp
// 策略 3: 无参数 + 代码文件 + SymbolManager 可用 → 返回符号摘要
if (symbolMgr && isCodeFile(filePath)) {
    // 如果符号扫描还在进行中，跳过符号摘要以避免阻塞
    if (symbolMgr->isScanning()) {
        std::cout << "[ReadCodeBlock] Symbol scan in progress, skipping summary (fallback to full file)" << std::endl;
    } else {
        std::cout << "[ReadCodeBlock] Strategy: Generate symbol summary" << std::endl;
        auto summary = generateSymbolSummary(filePath);
        if (!summary.contains("error")) {
            return summary;
        }
        std::cout << "[ReadCodeBlock] Symbol summary failed, fallback to full file" << std::endl;
    }
}
```

### 工作流程

**修复前**：
1. 用户请求读取文件
2. 工具尝试生成符号摘要
3. 调用 `getFileSymbols()` → **阻塞等待锁**
4. 用户看到卡顿

**修复后**：
1. 用户请求读取文件
2. **检查扫描状态** → 如果正在扫描，跳过符号摘要
3. 直接读取全文 → **立即响应**
4. 用户得到完整文件内容

## 性能对比

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 扫描完成后读取 | 快速（符号摘要） | 快速（符号摘要） |
| 扫描进行中读取 | **卡顿 5-30秒** | **立即响应（全文）** |
| 首次启动时读取 | **卡顿** | **立即响应** |

## 用户体验改进

### 修复前
```
用户: 读取 AgentRuntime.cpp
系统: [ReadCodeBlock] Strategy: Generate symbol summary
      <沉默 10 秒...>
      <用户以为程序卡死>
```

### 修复后
```
用户: 读取 AgentRuntime.cpp
系统: [ReadCodeBlock] Symbol scan in progress, skipping summary (fallback to full file)
      [ReadCodeBlock] Strategy: Read full file
      <立即返回文件内容>
```

## 实现细节

### 修改的文件

- **src/tools/CoreTools.cpp**
  - 在 `ReadCodeBlockTool::execute()` 中添加扫描状态检查
  - 如果 `symbolMgr->isScanning()` 返回 true，跳过符号摘要

### 关键 API

```cpp
// SymbolManager 提供的 API
bool SymbolManager::isScanning() const {
    return scanning;  // atomic<bool>，无需加锁
}
```

这个 API 是线程安全的（使用 `std::atomic<bool>`），不会阻塞。

## 未来优化

可以考虑的进一步改进：

1. **进度提示**
   ```cpp
   if (symbolMgr->isScanning()) {
       std::cout << "[ReadCodeBlock] Symbol scan in progress (" 
                 << symbolMgr->getProgress() << "%), please wait..." << std::endl;
   }
   ```

2. **非阻塞查询**
   - 实现 `tryGetFileSymbols()` 方法，使用 `try_lock` 而不是阻塞锁
   - 如果无法立即获取锁，返回空结果

3. **缓存机制**
   - 缓存最近查询的文件符号
   - 避免重复查询相同文件

4. **延迟符号摘要**
   - 首次请求返回全文
   - 后台异步生成符号摘要
   - 下次请求时使用缓存的摘要

## 测试

### 测试场景 1：扫描完成后读取
```bash
# 启动 Photon，等待扫描完成（约 5-10 秒）
./photon .

# 输入命令
> 给我讲一下 src/agent/AgentRuntime.cpp

# 预期：快速返回符号摘要
```

### 测试场景 2：扫描进行中读取
```bash
# 启动 Photon，立即输入命令（不等待扫描完成）
./photon .

# 立即输入
> 给我讲一下 src/agent/AgentRuntime.cpp

# 预期：立即返回全文（不卡顿）
```

## 总结

通过添加简单的扫描状态检查，解决了 `read_code_block` 工具的卡顿问题：

✅ **修复前**：扫描进行中读取文件会卡顿 5-30 秒
✅ **修复后**：扫描进行中立即返回全文，无卡顿
✅ **保持功能**：扫描完成后仍然使用符号摘要（更简洁）
✅ **用户体验**：响应速度大幅提升，无需等待

这是一个典型的**优雅降级**（graceful degradation）策略：
- 理想情况：使用符号摘要（更简洁）
- 降级情况：使用全文读取（仍然可用）
- 关键改进：避免阻塞等待
