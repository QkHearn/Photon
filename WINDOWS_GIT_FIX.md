# Windows Git Detection Fix

## 问题描述
在Windows上，Photon可能显示 "the system cannot find the path specified" 错误，即使 `where git` 可以找到Git。

## 原因分析
1. **重定向符号不兼容**：Windows的`system()`函数对`>/dev/null 2>&1`的处理与Unix不同
2. **命令格式差异**：Windows需要使用`>nul 2>nul`而不是`>/dev/null 2>&1`
3. **popen函数差异**：Windows需要使用`_popen`和`_pclose`

## 修复内容

### 1. Git检测函数优化
```cpp
// Windows兼容的Git检测函数
bool checkGitAvailable() {
#ifdef _WIN32
    // Windows: 首先检查git命令是否存在
    std::string gitPath = findExecutableInPath({"git"});
    if (gitPath.empty()) {
        std::cout << YELLOW << "  ⚠ Git command not found in PATH" << RESET << std::endl;
        std::cout << GRAY << "    - Run 'where git' to check Git location" << RESET << std::endl;
        std::cout << GRAY << "    - Make sure Git is installed and added to PATH" << RESET << std::endl;
        return false;
    }
    
    std::cout << GRAY << "  Git found at: " << gitPath << RESET << std::endl;
    
    // 检查是否在git仓库中
    int result = system("git rev-parse --is-inside-work-tree >nul 2>nul");
    if (result != 0) {
        std::cout << YELLOW << "  ⚠ Git command found but not in a Git repository" << RESET << std::endl;
        std::cout << GRAY << "    - Current directory: " << fs::current_path().u8string() << RESET << std::endl;
        std::cout << GRAY << "    - Run 'git init' to initialize a repository" << RESET << std::endl;
        return false;
    }
    
    return true;
#else
    // Unix/Linux/macOS: 使用原始命令
    return (system("git rev-parse --is-inside-work-tree >/dev/null 2>&1") == 0);
#endif
}
```

### 2. 命令重定向修复
```cpp
// Windows使用 >nul 2>nul
#ifdef _WIN32
    bool isDirty = (system("git diff --quiet 2>nul") != 0);
#else
    bool isDirty = (system("git diff --quiet 2>/dev/null") != 0);
#endif
```

### 3. popen函数修复
```cpp
#ifdef _WIN32
    FILE* pipe = _popen("git rev-parse --abbrev-ref HEAD 2>nul", "r");
#else
    FILE* pipe = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
#endif

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
```

### 4. Git恢复命令修复
```cpp
#ifdef _WIN32
    // Windows Git较老版本可能不支持git restore，使用git checkout
    success = (system(("git checkout -- \"" + lastFile + "\"").c_str()) == 0);
#else
    success = (system(("git restore \"" + lastFile + "\"").c_str()) == 0);
#endif
```

## 测试步骤

1. **检查Git安装**:
   ```cmd
   where git
   git --version
   ```

2. **测试Git检测**:
   ```cmd
   git clone https://github.com/your-repo/photon.git
   cd photon
   mkdir build && cd build
   cmake ..
   make
   ./photon ..
   ```

3. **验证输出**:
   - 应该显示 "Git environment detected, using Git for version control"
   - 如果Git未找到，显示 "Git command not found in PATH"
   - 如果不在Git仓库，显示 "Git command found but not in a Git repository"

## 环境变量设置
在Windows上，确保：
1. Git已安装（推荐使用Git for Windows）
2. Git的bin目录在PATH中（通常是`C:\Program Files\Git\bin`）
3. 重启命令行或IDE以使PATH更改生效

## 故障排除
如果仍然有问题：

1. **检查PATH**:
   ```cmd
   echo %PATH%
   ```

2. **手动测试命令**:
   ```cmd
   git rev-parse --is-inside-work-tree
   git status
   ```

3. **使用绝对路径测试**:
   ```cmd
   "C:\Program Files\Git\bin\git.exe" rev-parse --is-inside-work-tree
   ```

4. **检查权限**:
   - 确保有权限访问Git安装目录
   - 以管理员身份运行命令行

## 备用方案
如果Git检测仍然失败，Photon会自动降级到本地备份模式：
- 所有文件修改都会创建备份
- 撤销功能使用本地备份而非Git历史
- 功能完全可用，只是没有Git的版本控制优势