# Git优先的Apply Patch集成

## 🎯 功能概述

现在你的`apply_patch`工具已经升级为**Git优先**模式！它会：

1. **优先使用Git备份** - 使用`git stash`创建备份点
2. **优先使用Git跟踪** - 修改后自动`git add`文件
3. **智能回退机制** - Git不可用时自动降级到本地备份
4. **状态报告** - 清晰显示使用的备份模式

## 🔧 实现细节

### 核心改进

#### 1. Git备份策略
```cpp
bool ApplyPatchTool::createGitBackup(const std::string& path) {
    // 检查文件是否被Git跟踪
    std::string gitCheckCmd = "git ls-files --error-unmatch \"" + srcPath.u8string() + "\" >nul 2>nul";
    int result = system(gitCheckCmd.c_str());
    if (result != 0) {
        return false; // 文件未被Git跟踪
    }
    
    // 使用Git stash创建备份
    std::string stashCmd = "cd \"" + rootPath.u8string() + "\" && git stash push -m \"photon-backup-" + path + "\" -- \"" + srcPath.u8string() + "\" >nul 2>nul";
    result = system(stashCmd.c_str());
    return (result == 0);
}
```

#### 2. Git写入跟踪
```cpp
bool ApplyPatchTool::writeFileWithGit(const std::string& path, const std::vector<std::string>& lines) {
    // 写入文件...
    
    // 使用Git添加更改
    std::string gitAddCmd = "cd \"" + rootPath.u8string() + "\" && git add \"" + fullPath.u8string() + "\" >nul 2>nul";
    result = system(gitAddCmd.c_str());
    return (result == 0);
}
```

#### 3. 智能回退机制
```cpp
void ApplyPatchTool::createBackup(const std::string& path) {
    if (hasGit) {
        // 优先尝试Git备份
        if (createGitBackup(path)) {
            return; // Git备份成功
        }
    }
    
    // 回退到本地备份
    createLocalBackup(path);
}
```

## 📊 使用模式

### Git可用时的行为
1. **备份阶段**: 使用`git stash`创建备份
2. **修改阶段**: 正常进行行级编辑
3. **写入阶段**: 文件写入 + `git add`跟踪
4. **状态报告**: 显示"Git tracking enabled"

### Git不可用时的行为
1. **备份阶段**: 使用`.photon/backups/`本地备份
2. **修改阶段**: 正常进行行级编辑
3. **写入阶段**: 普通文件写入
4. **状态报告**: 显示"Local backup mode"

## 🧪 测试验证

### 测试环境检查
```bash
# 检查Git可用性
git rev-parse --is-inside-work-tree

# 检查文件Git跟踪状态
git ls-files --error-unmatch your-file.txt
```

### 使用示例
```json
{
  "type": "function",
  "function": {
    "name": "apply_patch",
    "arguments": {
      "file_path": "src/main.cpp",
      "operation": "replace",
      "start_line": 10,
      "end_line": 15,
      "content": "// New code here"
    }
  }
}
```

### 预期输出
```
✅ Successfully applied replace to src/main.cpp at lines 10 (Git tracking enabled)
```

## 🛡️ 安全特性

1. **双重备份**: Git stash + 本地备份双重保护
2. **文件跟踪检查**: 确保文件被Git跟踪才使用Git模式
3. **错误处理**: 任何Git操作失败都自动回退
4. **跨平台**: Windows/macOS/Linux兼容

## 🔍 故障排除

### Git备份失败的情况
- 文件未被Git跟踪
- Git命令执行失败
- 权限问题

### 解决方案
系统会自动降级到本地备份模式，确保功能始终可用。

### 验证Git集成
```bash
# 查看Git状态
git status

# 查看最近的stash
git stash list

# 查看文件历史
git log --oneline your-file.txt
```

## 🚀 重新编译

使用以下命令重新编译以启用Git优先功能：

```bash
cd build
cmake ..
make
```

现在你的`apply_patch`工具已经具备了智能的Git优先功能，会在可用时充分利用Git的强大版本控制能力！