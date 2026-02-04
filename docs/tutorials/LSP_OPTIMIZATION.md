# LSP 初始化优化

## 问题

用户输入 `tools` 命令时，会看到：
```
(Finishing engine initialization...)
```

这个等待时间可能长达 **5-30 秒**，取决于：
- 系统中安装的 LSP 服务器数量
- 每个 LSP 服务器的启动速度
- 网络连接状态

## 原因

Photon 在启动时会并行初始化三个组件：
1. **MCP 服务器** - 通常很快（< 1秒）
2. **技能加载** - 通常很快（< 1秒）
3. **LSP 服务器** - **最慢**（5-30秒）

LSP 初始化包括：
- 自动检测系统中的 LSP 服务器（clangd, pyright, typescript-language-server 等）
- 尝试启动每个检测到的 LSP 服务器
- 等待每个服务器的初始化响应

这些操作在后台异步执行，但当用户第一次输入命令时，系统需要等待所有初始化完成。

## 解决方案

添加了 `enable_lsp` 配置选项，允许用户禁用 LSP 功能以加快启动速度。

### 配置文件修改

在 `config.json` 中添加：

```json
{
  "agent": {
    "enable_lsp": false,  // 设置为 false 禁用 LSP，加快启动
    ...
  }
}
```

### 性能对比

| 配置 | 首次命令响应时间 | 功能影响 |
|------|-----------------|---------|
| `enable_lsp: true` | 5-30 秒 | 完整的代码导航功能（跳转到定义、查找引用等） |
| `enable_lsp: false` | **< 1 秒** | 仅使用 Tree-sitter 进行符号提取，无代码导航 |

## 使用建议

### 何时启用 LSP (`enable_lsp: true`)

✅ 适合以下场景：
- 需要精确的代码导航（跳转到定义、查找所有引用）
- 大型代码库，需要跨文件的符号解析
- 开发环境，需要完整的 IDE 功能
- 不介意启动时的等待时间

### 何时禁用 LSP (`enable_lsp: false`)

✅ 适合以下场景：
- **快速启动优先**
- 主要使用 Tree-sitter 进行符号提取（已足够准确）
- 小型项目或脚本
- 演示或测试环境
- 系统中没有安装 LSP 服务器

## Tree-sitter vs LSP

禁用 LSP 后，Photon 仍然可以使用 Tree-sitter 进行符号提取：

| 功能 | Tree-sitter | LSP |
|------|-------------|-----|
| 符号提取 | ✅ 快速、准确 | ✅ 最准确 |
| 跳转到定义 | ❌ | ✅ |
| 查找引用 | ❌ | ✅ |
| 启动速度 | ✅ 快（< 1秒） | ❌ 慢（5-30秒） |
| 跨文件解析 | ❌ | ✅ |

## 实现细节

### 修改的文件

1. **src/core/ConfigManager.h**
   - 添加 `bool enableLSP = true;` 字段
   - 添加配置读取逻辑

2. **src/core/main.cpp**
   - 在 LSP 初始化函数开始处检查 `cfg.agent.enableLSP`
   - 如果禁用，直接返回空结果，跳过所有 LSP 相关操作

3. **config.json**
   - 添加 `"enable_lsp": false` 配置项

### 代码示例

```cpp
auto lspInitFuture = std::async(std::launch::async, [&]() -> LspInitResult {
    LspInitResult result;
    
    // 如果 LSP 被禁用，直接返回空结果
    if (!cfg.agent.enableLSP) {
        return result;  // 立即返回，无需等待
    }
    
    // ... 正常的 LSP 初始化逻辑
});
```

## 验证

禁用 LSP 后，启动 Photon 并输入 `tools` 命令：
- ✅ 无需等待 "Finishing engine initialization..."
- ✅ 立即显示工具列表
- ✅ 符号扫描仍然正常工作（使用 Tree-sitter）

## 未来优化

可以考虑的进一步优化：
1. **延迟初始化**：只在真正需要 LSP 功能时才初始化
2. **超时机制**：为 LSP 初始化设置超时，避免无限等待
3. **缓存机制**：缓存已初始化的 LSP 服务器，避免重复启动
4. **按需加载**：只初始化当前项目需要的 LSP 服务器

## 总结

通过添加 `enable_lsp` 配置选项：
- ✅ 解决了启动慢的问题
- ✅ 保持了功能的灵活性
- ✅ 用户可以根据需求选择启用或禁用
- ✅ 默认禁用，优先保证启动速度

对于大多数使用场景，Tree-sitter 已经足够准确，禁用 LSP 是推荐的配置。
