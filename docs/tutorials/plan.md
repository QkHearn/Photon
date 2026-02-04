# Photon 重构计划 (Plan.md)

## 📋 重构初衷 (Why Refactor?)

### 🎯 核心问题诊断

当前的 Photon 项目**技术实力强大，但架构边界模糊**。主要问题：

#### 1️⃣ **决策层与执行层耦合**
```
问题：InternalMCPClient 既做决策又做执行
现状：
  - 40+ 个工具全在一个类里
  - 包含智能逻辑（contextPlan, toolReason, toolPlan）
  - LLM 直接调用底层能力（SymbolManager, LSPClient）

后果：
  ❌ LLM 看到太多复杂工具，决策质量下降
  ❌ 工具"太聪明"，智能体"失去控制"
  ❌ 无法区分"环境感知"和"工具执行"
```

#### 2️⃣ **缺少真正的 Agent Runtime**
```
问题：没有统一的智能体主循环
现状：
  - main.cpp 只是简单的消息转发
  - 没有 Plan → Act → Observe 循环
  - 没有状态管理和失败记忆

后果：
  ❌ 无法处理复杂多步任务
  ❌ 重复犯同样的错误
  ❌ 无法从失败中学习
```

#### 3️⃣ **上下文 ≠ 记忆**
```
问题：把"记忆"混在"上下文"里
现状：
  - 项目信息每次都塞进 prompt
  - 环境探测结果浪费 token
  - 失败经验无法积累

后果：
  ❌ Token 使用效率极低
  ❌ 上下文窗口很快耗尽
  ❌ 无法构建长期知识
```

#### 4️⃣ **工具不够"笨"**
```
问题：工具承担了太多智能
现状：
  - file_read 有多种模式（符号/窗口/范围）
  - tool_search 包含语义理解
  - 诊断、分析都在工具层

后果：
  ❌ 工具职责不清
  ❌ 难以测试和维护
  ❌ Agent 无法自主决策
```

---

## 🎯 重构目标 (Goals)

### ✅ **核心原则**

1. **工具必须极简极笨**
   - LLM 不能直接读文件、写文件、调用 LSP
   - 只能通过 4 个原子工具操作

2. **智能全在 Agent 层**
   - Symbol 索引 → Agent 内部
   - 调用图分析 → Agent 内部
   - 失败记忆 → Agent 内部

3. **记忆 ≠ 上下文**
   - ProjectMemory：项目类型、构建系统、架构约定
   - FailureMemory：错误 → 解决方案映射
   - 上下文只保留当前任务相关信息

4. **Plan → Act → Observe 循环**
   - 每个任务都有明确的生命周期
   - 失败自动记录，下次避免
   - 支持多步推理和回溯

---

## 📐 重构后的架构蓝图

### 🏗️ 新目录结构

```
src/
├── core/                          # 核心基础设施
│   ├── main.cpp                   # 入口（改造）
│   ├── LLMClient.h/cpp            # ✅ 保留（完美设计）
│   ├── ConfigManager.h            # ✅ 保留
│   ├── ContextManager.h/cpp       # ⚠️ 瘦身（只管压缩）
│   └── Logger.h/cpp               # ✅ 保留
│
├── agent/                         # 🆕 智能体核心层
│   ├── AgentRuntime.h/cpp         # 主循环：Plan → Act → Observe
│   ├── AgentState.h/cpp           # 任务状态管理
│   ├── PromptAssembler.h/cpp      # Prompt 组装器
│   └── EnvironmentDetector.h/cpp  # 环境自动探测
│
├── tools/                         # 🆕 工具执行层（极简）
│   ├── ToolRegistry.h/cpp         # 统一工具注册中心
│   ├── CoreTools.h/cpp            # 4 个 MVP 工具
│   └── ITool.h                    # 工具接口定义
│
├── memory/                        # 🆕 记忆系统
│   ├── MemoryManager.h/cpp        # 记忆统一入口
│   ├── ProjectMemory.h/cpp        # 项目知识库
│   ├── FailureMemory.h/cpp        # 失败案例库
│   └── UserPreference.h/cpp       # 用户偏好
│
├── analysis/                      # 🆕 分析引擎层（私有）
│   ├── SymbolManager.h/cpp        # ✅ 符号索引（从 utils/ 移来）
│   ├── SemanticManager.h/cpp      # ✅ 语义搜索（从 utils/ 移来）
│   ├── LSPClient.h/cpp            # ✅ LSP 客户端（从 mcp/ 移来）
│   ├── LogicMapper.h/cpp          # ✅ 调用图分析（亮点设计）
│   └── providers/
│       ├── ISymbolProvider.h      # 符号提取接口
│       ├── TreeSitterProvider.h/cpp  # ✅ AST 解析
│       └── RegexProvider.h/cpp       # ✅ 正则提取
│
├── mcp/                           # MCP 协议层
│   ├── MCPClient.h/cpp            # ✅ 外部 MCP 客户端
│   ├── MCPManager.h/cpp           # ✅ MCP 服务管理器
│   └── IMCPClient.h               # MCP 接口定义
│
└── skills/                        # 技能系统（角色转变）
    ├── SkillManager.h/cpp         # ⚠️ 改造（变成知识库）
    └── BuiltinSkillsData.h        # ✅ 内置技能数据
```

---

## 🚀 分阶段重构计划（6 步）

### **阶段 0：准备工作**（1 天）

#### 目标：备份现有代码，建立测试基线

```bash
# 1. 创建重构分支
git checkout -b refactor/agent-runtime

# 2. 备份关键文件
cp -r src src.backup

# 3. 记录当前功能清单
# 确保重构后所有功能仍可用
```

#### 产出：
- ✅ `src.backup/` 备份目录
- ✅ `FEATURES.md` 功能清单
- ✅ 测试基线（如果有单元测试）

---

### **阶段 1：新建目录和核心接口**（2 天）

#### 目标：搭建新架构骨架，不破坏现有代码

#### 任务清单：

1. **创建新目录结构**
```bash
mkdir -p src/agent src/tools src/memory src/analysis/providers
```

2. **定义核心接口**

```cpp
// src/tools/ITool.h
class ITool {
public:
    virtual ~ITool() = default;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual nlohmann::json getSchema() const = 0;
    virtual nlohmann::json execute(const nlohmann::json& args) = 0;
};
```

```cpp
// src/agent/AgentState.h
struct AgentState {
    std::string taskGoal;           // 用户目标
    std::string currentPhase;       // planning / executing / observing
    std::vector<std::string> completedSteps;
    std::vector<std::string> failedAttempts;
    nlohmann::json context;         // 当前任务上下文
};
```

3. **实现 ToolRegistry**

```cpp
// src/tools/ToolRegistry.h
class ToolRegistry {
public:
    void registerTool(std::unique_ptr<ITool> tool);
    ITool* getTool(const std::string& name);
    std::vector<nlohmann::json> listToolSchemas() const;
    nlohmann::json executeTool(const std::string& name, const nlohmann::json& args);
private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> tools;
};
```

#### 产出：
- ✅ `src/agent/AgentState.h`
- ✅ `src/tools/ITool.h`
- ✅ `src/tools/ToolRegistry.h/cpp`
- ✅ 编译通过（新文件不影响旧代码）

---

### **阶段 2：实现 4 个 MVP 工具**（2 天）

#### 目标：替换 InternalMCPClient 的核心功能

#### 2.1 实现工具类

```cpp
// src/tools/CoreTools.h
class ReadCodeBlockTool : public ITool {
public:
    ReadCodeBlockTool(const std::string& rootPath);
    nlohmann::json execute(const nlohmann::json& args) override;
    // Input:  { "file_path": "src/main.cpp", "start_line": 10, "end_line": 20 }
    // Output: { "content": "...", "total_lines": 100 }
};

class ApplyPatchTool : public ITool {
public:
    ApplyPatchTool(const std::string& rootPath);
    nlohmann::json execute(const nlohmann::json& args) override;
    // Input:  { "file_path": "src/main.cpp", "unified_diff": "..." }
    // Output: { "success": true, "applied_lines": [10, 11, 12] }
};

class RunCommandTool : public ITool {
public:
    RunCommandTool(const std::string& rootPath);
    nlohmann::json execute(const nlohmann::json& args) override;
    // Input:  { "command": "cmake --build build", "timeout": 30 }
    // Output: { "stdout": "...", "stderr": "...", "exit_code": 0 }
private:
    bool isCommandSafe(const std::string& cmd);
};

class ListProjectFilesTool : public ITool {
public:
    ListProjectFilesTool(const std::string& rootPath);
    nlohmann::json execute(const nlohmann::json& args) override;
    // Input:  { "path": "src", "max_depth": 2 }
    // Output: { "tree": [...], "total_files": 120 }
};
```

#### 2.2 从 InternalMCPClient 迁移代码

**迁移策略**：
- `fileRead` → `ReadCodeBlockTool::execute()`
- `fileWrite` → `ApplyPatchTool::execute()`
- `bashExecute` → `RunCommandTool::execute()`
- `listDirTree` → `ListProjectFilesTool::execute()`

**关键改造**：
- ❌ 删除所有"智能判断"逻辑（如 contextPlan 检查）
- ❌ 删除所有"读取模式"（符号、窗口、范围）
- ✅ 只保留最原子的操作

#### 2.3 注册工具

```cpp
// src/core/main.cpp (部分)
ToolRegistry registry;
registry.registerTool(std::make_unique<ReadCodeBlockTool>(rootPath));
registry.registerTool(std::make_unique<ApplyPatchTool>(rootPath));
registry.registerTool(std::make_unique<RunCommandTool>(rootPath));
registry.registerTool(std::make_unique<ListProjectFilesTool>(rootPath));
```

#### 产出：
- ✅ `src/tools/CoreTools.h/cpp`
- ✅ 4 个工具通过单元测试
- ✅ 与 InternalMCPClient 功能对等

---

### **阶段 3：实现 AgentRuntime 主循环**（3 天）

#### 目标：建立真正的智能体决策循环

#### 3.1 AgentRuntime 核心接口

```cpp
// src/agent/AgentRuntime.h
class AgentRuntime {
public:
    AgentRuntime(
        std::shared_ptr<LLMClient> llmClient,
        ToolRegistry& toolRegistry,
        SymbolManager& symbolManager,
        MemoryManager& memoryManager
    );

    // 主入口：执行用户任务
    void executeTask(const std::string& userGoal);

private:
    // 核心循环
    void runLoop();
    
    // 三个阶段
    void planPhase();      // 调用 LLM 生成计划
    void actPhase();       // 执行工具调用
    void observePhase();   // 分析结果，决定下一步
    
    // 内部能力（不暴露给 LLM）
    std::vector<Symbol> querySymbols(const std::string& query);
    std::optional<Symbol> findSymbol(const std::string& name);
    std::string symbolToLocation(const Symbol& sym);  // symbol → file:line
    
    // 失败处理
    void recordFailure(const std::string& toolName, 
                      const nlohmann::json& args, 
                      const std::string& error);
    bool hasSimilarFailure(const std::string& error);

private:
    std::shared_ptr<LLMClient> llm;
    ToolRegistry& tools;
    SymbolManager& symbolMgr;
    MemoryManager& memory;
    
    AgentState state;
    std::vector<nlohmann::json> messageHistory;
};
```

#### 3.2 核心循环实现

```cpp
void AgentRuntime::runLoop() {
    while (!state.isComplete && state.iteration < MAX_ITERATIONS) {
        state.currentPhase = "planning";
        planPhase();  // LLM 决策下一步
        
        state.currentPhase = "acting";
        actPhase();   // 执行工具
        
        state.currentPhase = "observing";
        observePhase();  // 分析结果
        
        state.iteration++;
    }
}
```

#### 3.3 关键设计点

**A. Symbol 查询在 Agent 内部**
```cpp
void AgentRuntime::planPhase() {
    // 1. 检查 LLM 是否需要某个符号
    if (llmMentionedSymbol("MyClass")) {
        // 2. Agent 内部查询
        auto sym = symbolMgr.findSymbol("MyClass");
        if (sym) {
            // 3. 转换为行号
            std::string hint = "MyClass is at " + 
                symbolToLocation(*sym);
            // 4. 添加到上下文提示，而非让 LLM 调用工具
            state.context["symbol_hints"] = hint;
        }
    }
    
    // 5. LLM 只看到: "read_code_block(file, start, end)"
    auto response = llm->call(assemblePrompt());
}
```

**B. 失败记忆集成**
```cpp
void AgentRuntime::actPhase() {
    for (auto& toolCall : state.plannedActions) {
        try {
            auto result = tools.executeTool(toolCall.name, toolCall.args);
            state.observations.push_back(result);
        } catch (const std::exception& e) {
            // 查询历史失败
            if (memory.hasSimilarFailure(e.what())) {
                auto solution = memory.getSolution(e.what());
                state.context["failure_hint"] = solution;
            }
            // 记录新失败
            memory.recordFailure(toolCall.name, toolCall.args, e.what());
        }
    }
}
```

#### 产出：
- ✅ `src/agent/AgentRuntime.h/cpp`
- ✅ `src/agent/PromptAssembler.h/cpp`
- ✅ 能够执行简单的多步任务

---

### **阶段 4：实现记忆系统**（2 天）

#### 目标：分离上下文和记忆，提升 token 效率

#### 4.1 ProjectMemory（项目知识）

```cpp
// src/memory/ProjectMemory.h
class ProjectMemory {
public:
    void load(const std::string& rootPath);
    void save();
    
    // 自动探测
    std::string getProjectType();      // "C++", "Python", etc.
    std::string getBuildSystem();      // "CMake", "Make", "Cargo"
    std::vector<std::string> getToolchain();  // ["gcc", "cmake"]
    
    // 手动记录
    void setArchitectureNote(const std::string& note);
    void addCodingConvention(const std::string& rule);

private:
    nlohmann::json data;
    std::string memoryPath;  // .photon/project_memory.json
};
```

#### 4.2 FailureMemory（失败案例）

```cpp
// src/memory/FailureMemory.h
class FailureMemory {
public:
    struct Failure {
        std::string toolName;
        nlohmann::json args;
        std::string error;
        std::string solution;  // 如何解决的
        std::time_t timestamp;
    };
    
    void recordFailure(const std::string& tool, 
                      const nlohmann::json& args,
                      const std::string& error);
    
    void recordSolution(const std::string& error, 
                       const std::string& solution);
    
    std::optional<std::string> findSimilarFailure(const std::string& error);

private:
    std::vector<Failure> failures;
    std::string memoryPath;  // .photon/failure_memory.json
};
```

#### 4.3 EnvironmentDetector（启动时自动运行）

```cpp
// src/agent/EnvironmentDetector.h
class EnvironmentDetector {
public:
    void detect(const std::string& rootPath, ProjectMemory& memory);

private:
    void detectProjectType();    // 查找 CMakeLists.txt, setup.py, etc.
    void detectBuildSystem();    // 测试 cmake, make, cargo
    void detectToolchain();      // 检查 gcc, clang, python
    void detectLanguageVersions();  // gcc --version, python --version
};
```

#### 4.4 集成到 main.cpp

```cpp
// src/core/main.cpp
int main() {
    // 1. 加载记忆
    MemoryManager memory(rootPath);
    
    // 2. 环境探测（只在首次或手动触发）
    if (!memory.project.exists()) {
        EnvironmentDetector detector;
        detector.detect(rootPath, memory.project);
        memory.project.save();
    }
    
    // 3. 创建 Agent
    AgentRuntime agent(llmClient, toolRegistry, symbolManager, memory);
    
    // 4. 执行任务
    agent.executeTask(userGoal);
}
```

#### 产出：
- ✅ `src/memory/MemoryManager.h/cpp`
- ✅ `src/memory/ProjectMemory.h/cpp`
- ✅ `src/memory/FailureMemory.h/cpp`
- ✅ `src/agent/EnvironmentDetector.h/cpp`
- ✅ 上下文 token 减少 30-50%

---

### **阶段 5：重组分析引擎**（2 天）

#### 目标：将 Symbol/LSP 移到 `analysis/`，作为 Agent 私有能力

#### 5.1 目录重组

```bash
# 移动文件
mv src/utils/SymbolManager.* src/analysis/
mv src/utils/SemanticManager.* src/analysis/
mv src/utils/LogicMapper.* src/analysis/
mv src/mcp/LSPClient.* src/analysis/

# 移动 providers
mv src/utils/TreeSitterSymbolProvider.* src/analysis/providers/
mv src/utils/RegexSymbolProvider.* src/analysis/providers/
```

#### 5.2 修改 #include 路径

```cpp
// 所有引用修改
#include "utils/SymbolManager.h"  →  #include "analysis/SymbolManager.h"
#include "mcp/LSPClient.h"        →  #include "analysis/LSPClient.h"
```

#### 5.3 确保不暴露给 LLM

```cpp
// ❌ 错误：作为工具暴露
registry.registerTool(new SymbolSearchTool());  // NO!

// ✅ 正确：作为 Agent 内部能力
class AgentRuntime {
private:
    SymbolManager& symbolMgr;  // Agent 内部持有
    // LLM 看不到
};
```

#### 产出：
- ✅ `src/analysis/` 目录完整
- ✅ 所有 #include 更新
- ✅ 编译通过
- ✅ LLM 无法直接访问分析能力

---

### **阶段 6：改造 SkillManager + 清理遗留代码**（2 天）

#### 目标：完成最后的清理工作

#### 6.1 SkillManager 角色转变

```cpp
// 从：执行者
class SkillManager {
    void executeSkill(const std::string& name);  // ❌ 删除
};

// 到：知识库
class SkillManager {
    // ✅ 只提供 Prompt 模板
    std::string getSkillPrompt(const std::string& name);
    
    // ✅ 建议的工具序列（但不执行）
    std::vector<std::string> suggestToolSequence(const std::string& skillName);
};
```

#### 6.2 删除遗留文件

```bash
# 删除已被替代的文件
rm src/mcp/InternalMCPClient.h
rm src/mcp/InternalMCPClient.cpp
rm src/utils/FileManager.h
rm src/utils/FileManager.cpp
rm src/core/UIManager.h
rm src/core/UIManager.cpp
```

#### 6.3 ContextManager 瘦身

```cpp
// src/core/ContextManager.h
class ContextManager {
public:
    // ✅ 保留：压缩功能
    nlohmann::json manage(const nlohmann::json& messages);
    nlohmann::json forceCompress(const nlohmann::json& messages);
    size_t getSize(const nlohmann::json& messages) const;

    // ❌ 删除：决策功能
    // bool shouldReadFile(const std::string& path);  // 删除
    // void planContext(const std::string& goal);     // 删除
};
```

#### 6.4 更新 CMakeLists.txt

```cmake
# 更新源文件列表
set(SOURCES
        ../../src/core/main.cpp
        ../../src/core/LLMClient.cpp
        ../../src/core/ContextManager.cpp
        ../../src/agent/AgentRuntime.cpp
        src/agent/PromptAssembler.cpp
        ../../src/tools/ToolRegistry.cpp
        ../../src/tools/CoreTools.cpp
        ../../src/memory/MemoryManager.cpp
        ../../src/analysis/SymbolManager.cpp
        ../../src/analysis/LSPClient.cpp
        # ...
)
```

#### 产出：
- ✅ 所有遗留代码清理完毕
- ✅ CMakeLists.txt 更新
- ✅ 编译通过
- ✅ 所有测试通过

---

## ✅ 重构完成后的验证清单

### 功能验证

- [ ] 能够读取任意代码文件
- [ ] 能够精确修改代码（patch 模式）
- [ ] 能够执行构建和测试
- [ ] 能够导航项目结构
- [ ] 能够完成多步任务
- [ ] 失败后能够自动恢复
- [ ] 记忆系统正常工作

### 架构验证

- [ ] LLM 只能看到 4 个工具
- [ ] Symbol/LSP 在 `analysis/` 目录
- [ ] AgentRuntime 有完整的 Plan→Act→Observe 循环
- [ ] ContextManager 只负责压缩
- [ ] SkillManager 变成知识库
- [ ] ProjectMemory 自动加载

### 性能验证

- [ ] 上下文 token 减少 30-50%
- [ ] 首次启动有环境探测
- [ ] 后续启动直接加载记忆
- [ ] 工具调用成功率 > 90%

---

## 📊 重构前后对比

### 代码结构对比

| 指标 | 重构前 | 重构后 | 改善 |
|-----|--------|--------|------|
| 工具数量 | 40+ | 4 | ⬇️ 90% |
| InternalMCPClient 行数 | ~2000 | 0（删除） | ⬇️ 100% |
| 模块耦合度 | 高（循环依赖） | 低（单向依赖） | ✅ |
| 决策层清晰度 | 模糊 | 清晰（AgentRuntime） | ✅ |

### Token 使用对比

| 场景 | 重构前 | 重构后 | 节省 |
|-----|--------|--------|------|
| 工具定义 | ~8000 tokens | ~1000 tokens | ⬇️ 87% |
| 项目信息 | 每次重复 | 存在 Memory | ⬇️ 100% |
| 环境信息 | 每次探测 | 首次探测 | ⬇️ 90% |

### 能力对比

| 能力 | 重构前 | 重构后 |
|-----|--------|--------|
| 多步推理 | ❌ 不支持 | ✅ 支持 |
| 失败学习 | ❌ 不支持 | ✅ 支持 |
| 环境感知 | ⚠️ 部分 | ✅ 完整 |
| 工具可测试性 | ⚠️ 困难 | ✅ 容易 |
| 代码可维护性 | ⚠️ 困难 | ✅ 清晰 |

---

## 🚨 风险与应对

### 风险 1：重构期间功能不可用
**应对**：
- 在新分支进行重构
- 保留 `src.backup/` 备份
- 每个阶段都要编译通过

### 风险 2：用户习惯改变
**应对**：
- 保持外部接口一致
- 命令行参数不变
- 配置文件兼容旧版

### 风险 3：性能下降
**应对**：
- 每个阶段做性能测试
- 保留 SymbolManager 的异步扫描
- LSP 并行初始化不变

---

## 📅 时间规划

| 阶段 | 工作量 | 时间 | 累计 |
|-----|--------|------|------|
| 阶段 0：准备 | 1 天 | Day 1 | 1 天 |
| 阶段 1：接口 | 2 天 | Day 2-3 | 3 天 |
| 阶段 2：工具 | 2 天 | Day 4-5 | 5 天 |
| 阶段 3：Runtime | 3 天 | Day 6-8 | 8 天 |
| 阶段 4：记忆 | 2 天 | Day 9-10 | 10 天 |
| 阶段 5：重组 | 2 天 | Day 11-12 | 12 天 |
| 阶段 6：清理 | 2 天 | Day 13-14 | 14 天 |
| **总计** | | **14 天** | |

---

## 🎯 成功标准

### 定量指标

- ✅ 工具数量从 40+ 降到 4
- ✅ Token 使用减少 50%
- ✅ 代码行数减少 30%
- ✅ 模块数量增加（但职责清晰）

### 定性指标

- ✅ 新人能快速理解架构
- ✅ 添加新工具只需实现 ITool 接口
- ✅ Agent 决策逻辑可单独测试
- ✅ 符合"极简工业级智能体"白皮书

---

## 📚 参考文档

- `design.md`：当前项目设计文档
- `极简工业级辅助编程智能体白皮书`：重构理论依据
- `src.backup/`：原始代码备份

---

## 🤝 后续演进方向

重构完成后，可以逐步增强：

1. **更多工具** (按需添加)
   - memory_read / memory_write
   - get_diagnostics

2. **更智能的 Agent**
   - 支持并行工具调用
   - 更复杂的失败恢复策略
   - 多轮交互优化

3. **更强的分析能力**
   - 增量编译集成
   - 更精确的调用图
   - 跨文件重构能力

---

**本重构计划由 Photon 团队制定，基于现有优秀设计，旨在"拉直神经，收紧边界"，而非推倒重来。**

**重构完成后，Photon 将成为真正的工业级辅助编程智能体核心引擎。**
