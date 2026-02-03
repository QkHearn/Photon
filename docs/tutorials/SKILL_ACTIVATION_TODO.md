# Skill 动态激活机制 - 实现清单

> 当前状态: 核心架构已完成,需要集成和测试

---

## 已完成 ✅

### 1. 核心架构

- [x] `SkillManager` 激活 API 设计
  - `bool activate(const std::string& name)`
  - `void deactivate(const std::string& name)`
  - `void deactivateAll()`
  - `bool isActive(const std::string& name)`
  - `std::vector<std::string> getActiveSkills()`

- [x] `SkillManager` Prompt 生成 API
  - `std::string getSkillDiscoveryPrompt()` (启动时注入)
  - `std::string getActiveSkillsPrompt()` (运行时注入)

- [x] `Skill` 结构扩展
  - 添加 `requiredTools` 字段
  - 添加 `constraints` 字段
  - 添加 `minimalInterface` 字段

- [x] `AgentRuntime` Skill 集成
  - 添加 `SkillManager*` 成员
  - 添加 `activateSkill()` / `deactivateSkill()` 方法
  - 构造函数支持 `SkillManager` 参数

- [x] Skill 激活/停用工具
  - `SkillActivateTool` 实现
  - `SkillDeactivateTool` 实现

- [x] Constitution 更新
  - 添加 Skill 激活规则 (§5.3)
  - 添加 Skill 生命周期 (§5.4)
  - 添加 Prompt 注入规则 (§5.5)

- [x] 文档
  - `SKILL_ACTIVATION_DESIGN.md` (设计文档)
  - `SKILL_ACTIVATION_EXAMPLE.md` (使用示例)
  - `SKILL_ACTIVATION_TODO.md` (本清单)

---

## 待实现 🚧

### 2. 核心功能完善

- [ ] **Skill 元数据解析**
  ```cpp
  // 从 SKILL.md 的 YAML frontmatter 解析:
  // - requiredTools
  // - constraints
  // - minimalInterface
  
  void SkillManager::parseFrontmatter(Skill& skill) {
      // 当前只解析 name 和 description
      // 需要扩展解析 tools, constraints 等
  }
  ```

- [ ] **AgentRuntime Prompt 注入实现**
  ```cpp
  void AgentRuntime::planPhase() {
      // TODO: 取消注释并实现
      if (skillMgr) {
          std::string activeSkillsPrompt = skillMgr->getActiveSkillsPrompt();
          if (!activeSkillsPrompt.empty()) {
              nlohmann::json skillPromptMsg;
              skillPromptMsg["role"] = "system";
              skillPromptMsg["content"] = activeSkillsPrompt;
              messageHistory.push_back(skillPromptMsg);
          }
      }
  }
  ```

- [ ] **初始 Prompt 注入**
  ```cpp
  std::string AgentRuntime::assembleSystemPrompt() {
      // TODO: 取消注释
      if (skillMgr) {
          prompt << skillMgr->getSkillDiscoveryPrompt();
      }
  }
  ```

- [ ] **ToolRegistry 集成**
  ```cpp
  // 在 main.cpp 或 ToolRegistry 初始化时:
  toolRegistry.registerTool(
      std::make_unique<SkillActivateTool>(&skillManager)
  );
  toolRegistry.registerTool(
      std::make_unique<SkillDeactivateTool>(&skillManager)
  );
  ```

### 3. 增强功能 (可选)

- [ ] **Skill 工具权限验证**
  ```cpp
  bool SkillManager::activate(const std::string& name) {
      // 检查 Skill 所需的工具是否在 ToolRegistry 中
      const Skill& skill = skills[name];
      for (const auto& tool : skill.requiredTools) {
          if (!toolRegistry->hasTool(tool)) {
              std::cerr << "Skill requires unavailable tool: " << tool << std::endl;
              return false;
          }
      }
      // ...
  }
  ```

- [ ] **Skill 依赖检查**
  ```cpp
  struct Skill {
      // ...
      std::vector<std::string> dependsOn;  // 依赖的其他 Skill
  };
  
  bool SkillManager::activate(const std::string& name) {
      // 自动激活依赖的 Skill
      const Skill& skill = skills[name];
      for (const auto& dep : skill.dependsOn) {
          if (!isActive(dep)) {
              activate(dep);
          }
      }
      // ...
  }
  ```

- [ ] **Skill 使用统计**
  ```cpp
  struct SkillStats {
      std::string name;
      int activationCount;
      std::chrono::milliseconds totalActiveTime;
      int toolCallCount;
  };
  
  std::map<std::string, SkillStats> skillStats;
  ```

- [ ] **Skill 热重载** (开发模式)
  ```cpp
  void SkillManager::reloadSkill(const std::string& name) {
      // 重新加载 Skill 文件
      // 更新激活状态下的 Skill
  }
  ```

---

## 集成步骤 📝

### Step 1: 完善 SkillManager 元数据解析

**文件**: `src/utils/SkillManager.h`

**任务**:
1. 扩展 `parseFrontmatter()` 解析更多字段
2. 支持从 SKILL.md 提取:
   - `tools: [list_files, read_code_block]`
   - `constraints: [read_only, max_500_lines]`
   - `interface: "Use list_files to scan, then read_code_block"`

**示例 SKILL.md 格式**:
```markdown
---
name: project_scan
description: Scan and analyze project structure
tools:
  - list_project_files
  - read_code_block
constraints:
  - Read-only access
  - Maximum 500 lines per read
interface: |
  1. Use list_project_files to get file list
  2. Use read_code_block to read specific files
---

# Project Scan Skill

(Full documentation...)
```

### Step 2: 取消 AgentRuntime 中的 TODO 注释

**文件**: `src/agent/AgentRuntime.cpp`

**任务**:
1. 取消 `assembleSystemPrompt()` 中的 Skill 发现 Prompt 注入
2. 取消 `planPhase()` 中的动态 Skill Prompt 注入
3. 确保 Prompt 注入逻辑正确

### Step 3: 注册 Skill 工具

**文件**: `src/core/main.cpp` (或工具注册代码所在位置)

**任务**:
```cpp
// 在初始化工具时添加:
toolRegistry.registerTool(
    std::make_unique<SkillActivateTool>(&skillManager)
);
toolRegistry.registerTool(
    std::make_unique<SkillDeactivateTool>(&skillManager)
);
```

### Step 4: 传递 SkillManager 到 AgentRuntime

**文件**: `src/core/main.cpp`

**任务**:
```cpp
// 创建 AgentRuntime 时传递 SkillManager
AgentRuntime agentRuntime(
    llmClient,
    toolRegistry,
    &symbolManager,  // 可选
    &memoryManager,  // 可选
    &skillManager    // 新增
);
```

### Step 5: 编译测试

```bash
cd /Users/hearn/Documents/code/demo/Photon
mkdir -p build && cd build
cmake ..
make

# 测试 Skill 激活
./photon --test-skill-activation
```

### Step 6: 创建测试 Skill

**位置**: `~/.photon/skills/test_skill/SKILL.md`

```markdown
---
name: test_skill
description: Test skill for activation demo
tools:
  - read_code_block
  - list_project_files
constraints:
  - Read-only access
  - Test mode only
interface: |
  This is a test skill.
  Use read_code_block to read files.
---

# Test Skill

This is a test skill for verifying the activation mechanism.
```

### Step 7: 端到端测试

**测试任务**:
```
用户: "使用 test_skill 读取项目文件"

预期行为:
1. Agent 调用 skill_activate("test_skill")
2. SkillManager 激活 Skill
3. Agent 收到 Skill Prompt 注入
4. Agent 使用 Skill 的工具完成任务
```

---

## 验证清单 ✓

在完成实现后,验证以下行为:

- [ ] **启动时 Skill 未激活**
  - 检查初始 Prompt 只包含 Skill 列表
  - 检查工具执行前 Skill 处于未激活状态

- [ ] **Skill 成功激活**
  - 调用 `skill_activate("test_skill")` 返回 success
  - `SkillManager.isActive("test_skill")` 返回 true
  - `getActiveSkills()` 包含 "test_skill"

- [ ] **Prompt 动态注入**
  - 激活后的 Prompt 包含 Skill 的 tools 和 constraints
  - Prompt 格式符合 `getActiveSkillsPrompt()` 规范

- [ ] **Skill 停用**
  - 调用 `skill_deactivate("test_skill")` 返回 success
  - `isActive("test_skill")` 返回 false

- [ ] **Allowlist 验证**
  - 激活不在 `config.json` 中的 Skill 失败
  - 错误消息提示 "Skill not in allowlist"

- [ ] **多 Skill 激活**
  - 同时激活 2-3 个 Skill
  - `getActiveSkillsPrompt()` 包含所有激活的 Skill

---

## 性能指标 📊

完成后测量:

```
场景: 10 个可用 Skill,平均激活 2 个

指标:
- 启动时 Prompt 大小: ~3K tokens
- 激活 2 个 Skill 后 Prompt 大小: ~4K tokens
- vs 全量注入: ~8K tokens
- 节省: 50%

场景: 100 轮对话

Token 消耗:
- 动态激活: ~400K tokens
- 全量注入: ~800K tokens
- 成本节省: $4.00 (假设 $0.01/1K tokens)
```

---

## 常见问题 ❓

### Q1: 为什么不在启动时激活所有 Skill?

**A**: 
- Token 浪费: 大部分 Skill 不会被用到
- 上下文污染: 过多 Skill 降低 LLM 决策质量
- 不可扩展: 企业级系统有数百个 Skill

### Q2: Skill 停用是必须的吗?

**A**: 
- 不是必须的,但推荐
- 优化成本: 及时释放不再使用的 Skill
- 简化上下文: 减少 LLM 需要考虑的信息

### Q3: 如何处理 Skill 依赖?

**A**: 
- 当前版本: 手动激活依赖的 Skill
- 增强版本: 自动检测和激活依赖 (见"增强功能")

### Q4: Prompt 注入会影响历史消息吗?

**A**: 
- 不会,Skill Prompt 作为新的 system 消息插入
- 不修改历史消息
- LLM 看到的是追加后的完整上下文

---

## 下一步 🚀

1. 完成"待实现"清单中的核心功能
2. 运行集成测试
3. 性能基准测试
4. 文档完善 (用户指南)
5. 考虑增强功能

---

**预计工作量**:
- 核心功能: 2-4 小时
- 测试验证: 1-2 小时
- 增强功能 (可选): 4-8 小时

**优先级**:
1. 🔥 Step 1-4 (核心集成)
2. ⚡ Step 5-7 (测试验证)
3. 💡 增强功能 (按需实现)
