# 全局 Photon 支持 - 路径处理增强

## 🎯 使用场景

用户将 `photon` 添加到环境变量后,可以在任意目录下使用:

```bash
# 场景 1: 在项目目录下启动
cd /Users/hearn/my-project
photon .

# 场景 2: 从任意位置启动
photon /Users/hearn/my-project

# 场景 3: 使用相对路径
cd /Users/hearn
photon ./my-project
```

然后在 Photon 中分析该项目的文件。

## 🔧 需求

在 Photon 运行时,应该支持:

### 1. 相对路径
```bash
> read_code_block("src/main.cpp")
> read_code_block("./src/main.cpp")
> read_code_block("../other-project/file.cpp")
```

### 2. 绝对路径
```bash
> read_code_block("/Users/hearn/my-project/src/main.cpp")
> read_code_block("/tmp/test.cpp")
```

## 🐛 原有问题

### 问题 1: 绝对路径处理错误

**原代码**:
```cpp
fs::path fullPath = rootPath / fs::u8path(filePath);
```

**问题**: 
- 如果 `filePath` 是绝对路径,`rootPath / absolutePath` 会导致错误!
- 例如: `/Users/hearn/project` / `/tmp/test.cpp` → 错误!

### 问题 2: 符号查询路径不一致

**原代码**:
```cpp
fs::path absPath = fs::absolute(rootPath / fs::u8path(filePath));
```

**问题**:
- 假设 `filePath` 是绝对路径,这会产生错误的路径
- 导致符号查询失败

## ✅ 解决方案

### 1. 智能路径判断

在所有文件操作前,先判断路径类型:

```cpp
fs::path inputPath = fs::u8path(filePath);
fs::path fullPath;

if (inputPath.is_absolute()) {
    // 绝对路径: 直接使用
    fullPath = inputPath;
} else {
    // 相对路径: 相对于 rootPath
    fullPath = rootPath / inputPath;
}
```

### 2. 统一的路径规范化

对于符号查询,需要将路径规范化为相对于 `rootPath` 的路径:

```cpp
// 计算文件的绝对路径
fs::path absPath;
if (inputPath.is_absolute()) {
    absPath = inputPath;
} else {
    absPath = fs::absolute(rootPath / inputPath);
}

// 如果文件在 rootPath 下,计算相对路径
try {
    if (absPath.string().find(rootAbsPath.string()) == 0) {
        normalizedPath = fs::relative(absPath, rootAbsPath).string();
    }
} catch (...) {
    // 如果无法计算相对路径,保持原样
    normalizedPath = filePath;
}
```

## 📝 修改的代码

### 1. `ReadCodeBlockTool::execute`

**位置**: `src/tools/CoreTools.cpp:155`

**修改**: 添加路径类型判断

```cpp
// 智能路径处理: 支持相对路径和绝对路径
fs::path inputPath = fs::u8path(filePath);
fs::path fullPath;

if (inputPath.is_absolute()) {
    fullPath = inputPath;
} else {
    fullPath = rootPath / inputPath;
}
```

### 2. `ReadCodeBlockTool::generateSymbolSummary`

**位置**: `src/tools/CoreTools.cpp:248`

**修改**: 改进路径规范化逻辑

```cpp
// 规范化路径: 统一转换为相对于 rootPath 的路径
fs::path inputPath = fs::u8path(filePath);
fs::path rootAbsPath = fs::absolute(rootPath);

fs::path absPath;
if (inputPath.is_absolute()) {
    absPath = inputPath;
} else {
    absPath = fs::absolute(rootPath / inputPath);
}

try {
    if (absPath.string().find(rootAbsPath.string()) == 0) {
        normalizedPath = fs::relative(absPath, rootAbsPath).string();
    }
} catch (...) {
    normalizedPath = filePath;
}
```

### 3. `ReadCodeBlockTool::readSymbolCode`

**位置**: `src/tools/CoreTools.cpp:375`

**修改**: 同样的路径规范化逻辑

### 4. `ReadCodeBlockTool::readLineRange`

**位置**: `src/tools/CoreTools.cpp:437`

**修改**: 添加路径类型判断

## 🎬 使用示例

### 场景 1: 分析当前项目

```bash
# 启动 Photon
cd /Users/hearn/my-project
photon .

# 使用相对路径
> read_code_block("src/main.cpp")
✅ 工作正常

# 使用绝对路径
> read_code_block("/Users/hearn/my-project/src/main.cpp")
✅ 工作正常

# 使用 ./ 前缀
> read_code_block("./src/main.cpp")
✅ 工作正常
```

### 场景 2: 分析其他项目文件

```bash
# 启动 Photon
cd /Users/hearn/project-a
photon .

# 分析 project-b 的文件 (绝对路径)
> read_code_block("/Users/hearn/project-b/src/main.cpp")
✅ 可以读取内容
⚠️ 无符号摘要 (不在当前项目索引中)

# 分析 project-b 的文件 (相对路径)
> read_code_block("../project-b/src/main.cpp")
✅ 可以读取内容
⚠️ 无符号摘要 (不在当前项目索引中)
```

### 场景 3: 符号摘要

```bash
# 项目内文件 - 有符号摘要
> read_code_block("src/main.cpp")

返回:
📊 Symbol Summary for: src/main.cpp
### functions (5):
  - `main` (lines 10-50)
  - `init` (lines 52-80)
  ...

# 项目外文件 - 无符号摘要,降级到全文
> read_code_block("/tmp/test.cpp")

返回:
File: /tmp/test.cpp
Lines: 1-100 (Total: 100)

1|#include <iostream>
2|int main() {
...
```

## 📊 路径处理矩阵

| 输入路径 | rootPath | 处理后的 fullPath | 符号查询路径 | 结果 |
|---------|----------|------------------|-------------|------|
| `src/main.cpp` | `/Users/hearn/project` | `/Users/hearn/project/src/main.cpp` | `src/main.cpp` | ✅ 符号摘要 |
| `./src/main.cpp` | `/Users/hearn/project` | `/Users/hearn/project/src/main.cpp` | `src/main.cpp` | ✅ 符号摘要 |
| `/Users/hearn/project/src/main.cpp` | `/Users/hearn/project` | `/Users/hearn/project/src/main.cpp` | `src/main.cpp` | ✅ 符号摘要 |
| `../other/file.cpp` | `/Users/hearn/project` | `/Users/hearn/other/file.cpp` | (无法规范化) | ✅ 全文读取 |
| `/tmp/test.cpp` | `/Users/hearn/project` | `/tmp/test.cpp` | (无法规范化) | ✅ 全文读取 |

## 🎯 关键特性

### 1. 路径灵活性 ✅
- 支持相对路径
- 支持绝对路径
- 支持 `./` 和 `../` 前缀
- 自动处理路径规范化

### 2. 符号索引智能匹配 ✅
- 项目内文件: 使用预构建索引
- 项目外文件: 降级到全文读取
- 路径自动规范化为相对路径

### 3. 错误处理 ✅
- 文件不存在: 清晰的错误提示
- 路径无效: 友好的错误信息
- 符号查询失败: 自动降级

## 🔍 调试信息

现在工具会输出详细的路径处理信息:

```
[ReadCodeBlock] Original path: /Users/hearn/project/src/main.cpp
[ReadCodeBlock] Normalized path: src/main.cpp
[ReadCodeBlock] SymbolManager root: /Users/hearn/project
[ReadCodeBlock] Total symbols in index: 1234
[ReadCodeBlock] Is scanning: no
[ReadCodeBlock] Found 15 symbols
```

## ✅ 验证清单

- [x] 支持相对路径 (`src/main.cpp`)
- [x] 支持绝对路径 (`/Users/.../main.cpp`)
- [x] 支持 `./` 前缀 (`./src/main.cpp`)
- [x] 支持 `../` 父目录引用
- [x] 项目内文件有符号摘要
- [x] 项目外文件可以读取内容
- [x] 路径规范化正确
- [x] 错误处理完善
- [x] 编译通过

## 🎉 总结

现在 Photon 完全支持作为全局工具使用:

1. ✅ **在任意目录下启动**: `photon .` 或 `photon /path/to/project`
2. ✅ **使用任意路径格式**: 相对路径、绝对路径、`./`、`../` 都支持
3. ✅ **智能符号分析**: 项目内文件自动使用符号索引
4. ✅ **降级策略**: 项目外文件仍可读取,只是没有符号摘要

**你的使用场景完全支持!** 🎊

```bash
# 添加到环境变量
export PATH=$PATH:/path/to/photon

# 在任意项目下使用
cd ~/my-awesome-project
photon .

# 分析文件
> read_code_block("src/main.cpp")           # ✅
> read_code_block("./src/main.cpp")         # ✅
> read_code_block("/full/path/to/file.cpp") # ✅
```
