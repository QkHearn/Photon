# Skill 动态激活 - 快速参考

> **一句话回答**: Skill 在运行时由 LLM 决策激活,激活时才注入 Prompt

---

## 核心概念

```
配置文件 (config.json)
  ↓ 定义 Allowlist
启动时 (Agent Init)
  ↓ 加载元数据,注入发现列表
运行时 (LLM 决策)
  ↓ 调用 skill_activate(name)
激活后 (Dynamic Injection)
  ↓ 注入工具、约束、接口
使用中 (Task Execution)
  ↓ LLM 使用 Skill 工具
停用后 (Optional)
  ↓ 调用 skill_deactivate(name)
```

---

## 三个关键 API

### 1. 发现 Prompt (启动时)

```cpp
std::string SkillManager::getSkillDiscoveryPrompt() const;
```

**输出**:
```
# Available Skills (NOT active):
- project_scan: Scan project files
- code_modify: Modify code files

⚠️  Must activate via skill_activate(name)
```

**何时调用**: Agent 初始化,组装 System Prompt 时

**作用**: 告诉 LLM 有哪些 Skill 可用

---

### 2. 激活 (运行时)

```cpp
bool SkillManager::activate(const std::string& name);
```

**调用链**:
```
LLM → skill_activate(name) → SkillActivateTool 
    → SkillManager.activate(name) → 返回成功
```

**验证**:
- ✅ Skill 在 Allowlist 中
- ✅ Skill 已加载
- ❌ Skill 不存在 → 返回 false

---

### 3. 激活 Prompt (动态注入)

```cpp
std::string SkillManager::getActiveSkillsPrompt() const;
```

**输出**:
```
# ACTIVATED SKILLS

## Skill: project_scan
**Allowed Tools**:
  - list_project_files
  - read_code_block

**Constraints**:
  - Read-only access
  - Max 500 lines per read
```

**何时调用**: Skill 激活后,下一次 Plan Phase 前

**作用**: 注入 Skill 的工具和约束

---

## 时序图

```
Agent 启动
  │
  ├─ loadSkills() → 元数据加载
  ├─ getSkillDiscoveryPrompt() → 注入发现列表
  └─ 初始化完成
  
用户请求
  │
Iteration 1: 决策阶段
  │
  ├─ LLM 分析任务
  ├─ LLM 决定需要 Skill
  └─ LLM 调用 skill_activate("project_scan")
  
Iteration 1: 执行阶段
  │
  ├─ SkillManager.activate("project_scan")
  ├─ 检查 Allowlist ✓
  └─ 返回成功
  
Iteration 2: 决策阶段 (动态注入)
  │
  ├─ getActiveSkillsPrompt() → 注入 Skill Prompt
  ├─ LLM 看到 Skill 工具
  └─ LLM 调用 Skill 工具
  
Iteration 2: 执行阶段
  │
  ├─ 执行 Skill 工具
  └─ 返回结果
  
Iteration N: 完成
  │
  └─ (可选) skill_deactivate("project_scan")
```

---

## 代码集成清单

### ✅ Step 1: 注册 Skill 工具

```cpp
// src/core/main.cpp
toolRegistry.registerTool(
    std::make_unique<SkillActivateTool>(&skillManager)
);
toolRegistry.registerTool(
    std::make_unique<SkillDeactivateTool>(&skillManager)
);
```

### ✅ Step 2: 传递 SkillManager

```cpp
// src/core/main.cpp
AgentRuntime agentRuntime(
    llmClient,
    toolRegistry,
    &symbolManager,
    &memoryManager,
    &skillManager    // 新增
);
```

### ✅ Step 3: 启用发现 Prompt

```cpp
// src/agent/AgentRuntime.cpp
std::string AgentRuntime::assembleSystemPrompt() {
    // ...
    if (skillMgr) {
        prompt << skillMgr->getSkillDiscoveryPrompt();
    }
    return prompt.str();
}
```

### ✅ Step 4: 启用动态注入

```cpp
// src/agent/AgentRuntime.cpp
void AgentRuntime::planPhase() {
    if (skillMgr) {
        std::string activeSkillsPrompt = skillMgr->getActiveSkillsPrompt();
        if (!activeSkillsPrompt.empty()) {
            nlohmann::json skillMsg;
            skillMsg["role"] = "system";
            skillMsg["content"] = activeSkillsPrompt;
            messageHistory.push_back(skillMsg);
        }
    }
    // 调用 LLM...
}
```

---

## 典型使用场景

### 场景 1: 简单任务 (单 Skill)

```
用户: "读取 main.cpp 文件"

Iteration 1:
  LLM: skill_activate("file_operations")
  
Iteration 2:
  LLM: read_code_block("main.cpp")
  
Result: 完成
```

### 场景 2: 复杂任务 (多 Skill)

```
用户: "扫描项目并修改所有 TODO"

Iteration 1:
  LLM: skill_activate("project_scan")
  
Iteration 2:
  LLM: list_project_files()
  
Iteration 3:
  LLM: skill_activate("code_modify")
  
Iteration 4-N:
  LLM: apply_patch() × N
  
Result: 完成
```

### 场景 3: 阶段切换 (Skill 停用)

```
用户: "实现功能 + 测试 + 文档"

Phase 1: 实现
  skill_activate("feature_dev")
  ... 开发 ...
  skill_deactivate("feature_dev")

Phase 2: 测试
  skill_activate("test_runner")
  ... 测试 ...
  skill_deactivate("test_runner")

Phase 3: 文档
  skill_activate("documentation")
  ... 文档 ...
  skill_deactivate("documentation")
```

---

## Token 成本对比

| Skill 数量 | 全量注入 | 动态激活 | 节省 |
|-----------|---------|---------|------|
| 10 | 5K | 1K | 80% |
| 50 | 25K | 2K | 92% |
| 200 | 100K | 3K | 97% |

**结论**: Skill 越多,动态激活优势越明显

---

## 常见问题

### Q: 为什么不全部注入?
**A**: Token 浪费,上下文污染,不可扩展

### Q: LLM 如何知道何时激活?
**A**: 发现 Prompt 告知可用 Skill,LLM 自主决定

### Q: 可以同时激活多个 Skill 吗?
**A**: 可以,`getActiveSkillsPrompt()` 返回所有激活 Skill 的 Prompt

### Q: 必须停用 Skill 吗?
**A**: 不必须,但推荐 (优化成本)

### Q: Skill 激活失败怎么办?
**A**: 返回错误,LLM 可以尝试其他方案

---

## 关键文件

```
设计文档:
  SKILL_ACTIVATION_DESIGN.md      - 完整设计
  SKILL_ACTIVATION_EXAMPLE.md     - 使用示例
  SKILL_ACTIVATION_SUMMARY.md     - 总结
  SKILL_ACTIVATION_TODO.md        - 实现清单
  SKILL_ACTIVATION_QUICK_REF.md   - 快速参考 (本文档)

核心代码:
  src/utils/SkillManager.h        - Skill 管理器
  src/tools/CoreTools.h/.cpp      - Skill 工具实现
  src/agent/AgentRuntime.h/.cpp   - Agent 集成

配置:
  photon_agent_constitution_v_2.md - Constitution 更新 (§5.3-5.5)
```

---

## 一句话总结

**Skill 在配置层定义 Allowlist,启动时加载元数据并注入发现列表,运行时由 LLM 决策激活,激活后动态注入工具和约束,这是工业级 Agent 的标准做法。**
