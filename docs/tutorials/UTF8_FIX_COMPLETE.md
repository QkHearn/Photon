# UTF-8 编码修复完成

## 问题描述

用户在执行 `CoreTools::read_code_block` 读取 `README.md` 文件时遇到致命错误：

```
FATAL ERROR: [json.exception.type_error.316] incomplete UTF-8 string; last byte: 0xA2
```

## 根本原因

1. **文件读取问题**：`ReadCodeBlockTool::execute()` 方法在读取文件时，没有正确处理可能存在的不完整或无效的 UTF-8 字节序列
2. **JSON 序列化检查**：nlohmann/json 库在序列化时会严格验证 UTF-8 编码的完整性，如果发现不完整的 UTF-8 序列会抛出异常
3. **错误传播**：不完整的 UTF-8 序列从文件读取传递到 JSON 对象构建，最终导致程序崩溃

## 解决方案

### 1. 添加 UTF-8 验证和清理工具 (`src/tools/CoreTools.h` 和 `CoreTools.cpp`)

在 `CoreTools.h` 中添加了 UTF-8 工具命名空间：

```cpp
namespace UTF8Utils {
    /**
     * @brief 验证并清理 UTF-8 字符串，替换无效字节为 '?'
     */
    std::string sanitize(const std::string& input);
}
```

在 `CoreTools.cpp` 中实现了严格的 UTF-8 验证逻辑：

- **单字节 ASCII** (0x00-0x7F)：直接保留
- **2 字节序列** (0xC0-0xDF)：验证续字节有效性
- **3 字节序列** (0xE0-0xEF)：验证所有续字节有效性
- **4 字节序列** (0xF0-0xF7)：验证所有续字节有效性
- **无效字节**：替换为 '?'

### 2. 修改文件读取逻辑 (`ReadCodeBlockTool::execute`)

**修改前**：
```cpp
std::ifstream file(fullPath);
std::string line;
while (std::getline(file, line)) {
    lines.push_back(line);
}
```

**修改后**：
```cpp
// 以二进制模式打开，避免文本模式的编码转换
std::ifstream file(fullPath, std::ios::binary);
std::string line;
while (std::getline(file, line)) {
    // 移除 Windows 风格的 \r
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    // 验证并清理 UTF-8
    lines.push_back(UTF8Utils::sanitize(line));
}
```

### 3. 最终内容验证

在构建 JSON 结果之前，对整个内容字符串进行最终验证：

```cpp
std::string finalContent = "File: " + filePath + "\n" +
                           "Lines: " + std::to_string(startLine) + "-" + std::to_string(endLine) + 
                           " (Total: " + std::to_string(totalLines) + ")\n\n" +
                           content.str();
finalContent = UTF8Utils::sanitize(finalContent);

nlohmann::json contentItem;
contentItem["type"] = "text";
contentItem["text"] = finalContent;
```

## 技术细节

### UTF-8 编码结构

- **1 字节**: `0xxxxxxx` (0x00-0x7F)
- **2 字节**: `110xxxxx 10xxxxxx` (起始字节 0xC0-0xDF，续字节 0x80-0xBF)
- **3 字节**: `1110xxxx 10xxxxxx 10xxxxxx` (起始字节 0xE0-0xEF)
- **4 字节**: `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` (起始字节 0xF0-0xF7)

### 处理策略

1. **逐字节扫描**：按照 UTF-8 规范验证每个字节序列
2. **续字节验证**：确保多字节序列的续字节符合 `10xxxxxx` 模式
3. **边界检查**：防止在字符串末尾出现不完整的多字节序列
4. **安全替换**：将无效字节替换为 ASCII 字符 '?'，保证输出始终是有效 UTF-8

## 影响范围

- ✅ `ReadCodeBlockTool` - 文件读取工具
- ✅ 所有使用 `read_code_block` 的功能
- ✅ JSON 序列化的稳定性
- ✅ 文件内容显示的完整性

## 测试建议

1. **基础测试**：读取包含中文的 README.md 文件
2. **边界测试**：读取包含各种编码混合的文件
3. **错误恢复**：验证无效编码被正确替换为 '?'
4. **性能测试**：验证 UTF-8 清理不会显著影响读取性能

## 编译状态

- ✅ 修复已编译通过
- ✅ 二进制文件已更新: `/Users/hearn/Documents/code/demo/Photon/build/photon`
- ✅ 编译时间: 2026-02-03 20:24

## 后续改进

可以考虑：

1. **性能优化**：对于已知是纯 ASCII 的文件跳过 UTF-8 验证
2. **编码检测**：自动检测文件编码（UTF-8, GBK, Latin-1 等）并转换
3. **用户通知**：当检测到无效编码时，向用户报告警告信息
4. **配置选项**：允许用户选择对无效编码的处理策略（替换、跳过、报错）

## 结论

通过实现严格的 UTF-8 验证和清理机制，解决了文件读取时可能遇到的编码问题，确保了 JSON 序列化的稳定性和程序的健壮性。修复后的代码能够优雅地处理各种编码场景，包括不完整或无效的 UTF-8 序列。
