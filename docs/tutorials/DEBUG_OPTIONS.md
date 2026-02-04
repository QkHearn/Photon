# Photon 调试选项

## 环境变量

Photon 支持通过环境变量启用详细的调试日志。

### PHOTON_DEBUG_SCAN

启用符号扫描的详细日志。

**用法:**
```bash
PHOTON_DEBUG_SCAN=1 ./photon
```

**输出示例:**
```
[SymbolManager] Starting async scan thread
[SymbolManager] Starting full scan of: .
[SymbolManager] Providers registered: 2
[SymbolManager] Scanning: ./src/agent/AgentRuntime.cpp (ext: .cpp)
[SymbolManager] Scanning: ./src/core/main.cpp (ext: .cpp)
...
[scanFile] File: src/agent/AgentRuntime.cpp, ext: '.cpp', providers: 2
[scanFile]   -> TreeSitter provider supports this extension
[scanFile]   -> Regex provider supports this extension
[scanFile]   supported=1, lspSupported=1
...
[SymbolManager] Scan complete: 250 files found, 85 scanned, 165 ignored, 1247 symbols extracted
[SymbolManager] Index saved
```

---

### PHOTON_DEBUG_READ

启用 ReadCodeBlock 工具的详细日志。

**用法:**
```bash
PHOTON_DEBUG_READ=1 ./photon
```

**输出示例:**
```
[ReadCodeBlock] === Path Normalization Debug ===
[ReadCodeBlock] Original path: src/agent/AgentRuntime.cpp
[ReadCodeBlock] SymbolManager root: .
[ReadCodeBlock] Root absolute path: /Users/hearn/Documents/code/demo/Photon
[ReadCodeBlock] File absolute path: /Users/hearn/Documents/code/demo/Photon/src/agent/AgentRuntime.cpp
[ReadCodeBlock] Is input absolute? no
[ReadCodeBlock] Path is under root, converted to relative: src/agent/AgentRuntime.cpp
[ReadCodeBlock] Final normalized path: src/agent/AgentRuntime.cpp
[ReadCodeBlock] Total symbols in index: 1247
[ReadCodeBlock] Is scanning: no
[ReadCodeBlock] Query for 'src/agent/AgentRuntime.cpp' returned 17 symbols
```

---

### 组合使用

可以同时启用多个调试选项：

```bash
PHOTON_DEBUG_SCAN=1 PHOTON_DEBUG_READ=1 ./photon
```

---

## 正常模式

在正常模式下（不设置环境变量），只会显示关键信息：

```
[SymbolManager] Scan complete: 250 files found, 85 scanned, 165 ignored, 1247 symbols extracted
```

这样可以保持终端输出清洁，同时仍然能看到扫描结果。

---

## 何时使用调试模式

### 使用 PHOTON_DEBUG_SCAN 的场景

1. **符号索引为空** - 检查哪些文件被扫描
2. **扫描速度慢** - 查看扫描进度
3. **某些文件未被索引** - 检查是否被跳过
4. **Provider 问题** - 查看哪个 provider 在工作

### 使用 PHOTON_DEBUG_READ 的场景

1. **ReadCodeBlock 找不到符号** - 检查路径转换
2. **路径匹配问题** - 查看规范化过程
3. **符号查询失败** - 查看索引状态

---

## 性能影响

调试日志会略微降低性能：

- **PHOTON_DEBUG_SCAN**: 扫描时间增加 ~5-10%（主要是 I/O 开销）
- **PHOTON_DEBUG_READ**: 查询时间增加 ~1-2ms（可忽略）

建议只在需要调试时启用。

---

## 日志输出位置

- 正常日志 → `stdout`
- 错误日志 → `stderr`

可以重定向到文件：

```bash
# 只保存正常日志
PHOTON_DEBUG_SCAN=1 ./photon > scan.log

# 只保存错误日志
PHOTON_DEBUG_SCAN=1 ./photon 2> errors.log

# 保存所有日志
PHOTON_DEBUG_SCAN=1 ./photon > all.log 2>&1
```

---

## 未来计划

计划添加更多调试选项：

- `PHOTON_DEBUG_LSP` - LSP 通信日志
- `PHOTON_DEBUG_SEMANTIC` - 语义搜索日志
- `PHOTON_DEBUG_TOOLS` - 工具调用日志
- `PHOTON_LOG_LEVEL` - 统一的日志级别控制

---

## 相关文档

- `SYMBOL_SCAN_FIX_COMPLETE.md` - 符号扫描修复详情
- `SYMBOL_STRATEGY_DESIGN.md` - 符号读取策略设计
- `test_symbol_scan.sh` - 测试脚本
