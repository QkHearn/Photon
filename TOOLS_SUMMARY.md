# Photon 工具列表

Photon 拥有分层工具架构，包含**核心工具(Core Tools)**和**MCP工具**两大类。

## 🛠️ 核心工具 (Core Tools)

这些是最基础的原子工具，在 `src/tools/CoreTools.h` 中定义：

### 1. **read_code_block** - 智能代码读取
- **功能**: 智能读取代码文件，根据需求自动选择最佳策略
- **智能特性**:
  - 无参数 + 代码文件 → 返回符号摘要
  - 指定 `symbol_name` → 返回符号代码
  - 指定 `start_line/end_line` → 返回指定行范围
  - 其他情况 → 返回全文
- **用途**: 代码审查、符号查找、行级编辑

### 2. **apply_patch** - 统一补丁应用（仅 unified diff）
- **功能**: **只接受 Git unified diff**，一次可更新多个文件、多处修改
- **输入**: `diff_content`（必填），可选 `backup`、`dry_run`
- **Git优先**: 有 Git 时走 `git apply` / `git apply --check`，并把最近补丁保存到 `.photon/patches/last.patch` 供 `undo` 反向应用
- **无Git回退**: 没有 Git 时使用内置 unified-diff 引擎按 hunk 应用（要求上下文严格匹配）

### 3. **run_command** - 命令执行
- **功能**: 执行系统命令
- **特性**: 极简设计，只执行命令不做过滤
- **安全检查**: 由Agent层完成安全验证
- **用途**: 构建、测试、系统操作

### 4. **list_project_files** - 项目文件浏览
- **功能**: 列出项目目录结构
- **特性**: 支持递归目录遍历和深度控制
- **用途**: 项目结构探索、文件发现

### 5. **skill_activate** - 技能激活
- **功能**: 动态激活指定的Skill
- **用途**: 按需加载专业技能

### 6. **skill_deactivate** - 技能停用
- **功能**: 停用指定的Skill，释放上下文
- **用途**: 资源管理、上下文清理

### 7. （已移除）
- `git_diff` 工具已移除，能力统一收敛到 `apply_patch`

## 🔌 MCP工具 (外部工具)

通过MCP (Model Context Protocol) 接入的外部工具，在配置中定义。

### 当前配置中的工具
根据 `tests/test_config.json` 配置，支持的工具包括：

#### 内置工具 (当 `use_builtin_tools: true`)
- **搜索工具**: Web搜索、代码搜索
- **内存工具**: 长期记忆存储和检索
- **任务调度**: 定时任务管理

#### 外部MCP服务器工具
- **文件系统工具**: 高级文件操作
- **数据库工具**: 数据库查询和管理
- **API工具**: 外部API调用
- **专业工具**: 领域特定工具

## 🎯 工具架构特点

### 极简设计原则
1. **单一职责**: 每个工具只做一件事，做到极致简单
2. **原子操作**: 工具之间可以组合使用，避免复杂逻辑
3. **智能组合**: Agent层负责智能组合工具完成复杂任务

### 安全机制
1. **Constitution校验**: 所有工具调用都通过宪法验证
2. **人工确认**: 高风险操作需要用户确认
3. **权限控制**: 细粒度的工具权限管理

### 跨平台支持
- ✅ Windows (包括Windows Git修复)
- ✅ macOS
- ✅ Linux
- 自动检测平台并适配命令格式

## 📊 工具统计

- **核心工具**: 6个（`apply_patch` 统一补丁；不再提供 `git_diff`）
- **MCP工具**: 根据配置动态加载
- **总工具数**: 核心工具 + 动态MCP工具

## 🔧 工具使用示例

### 在CLI中查看工具
```
❯ tools
```

### 工具调用示例
```json
{
  "type": "function",
  "function": {
    "name": "read_code_block",
    "arguments": {
      "path": "src/main.cpp",
      "mode": {
        "type": "symbol",
        "name": "main"
      }
    }
  }
}
```

## 🚀 扩展工具

新的工具可以通过以下方式添加：
1. **核心工具**: 继承 `ITool` 接口，注册到 `ToolRegistry`
2. **MCP工具**: 配置MCP服务器连接
3. **技能工具**: 通过Skill系统动态加载

工具系统采用插件化架构，支持运行时动态加载和卸载。