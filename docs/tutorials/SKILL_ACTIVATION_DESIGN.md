# Photon Skill 动态激活机制设计

> Status: Implementation Guide
> Date: 2026-02-03
> Author: Architecture Review

---

## 0. 核心问题

**问题**: 配置文件允许了 Skill,但什么时候真正加载/注入到 Prompt 里?

**答案**: Skill 激活是"运行期决定"的,不是启动时注入。

---

## 1. 设计原则

### 1.1 关键原则

```
配置文件 = Allowlist (允许列表)
Skill 激活 = Runtime Decision (运行时决策)
Prompt 注入 = Just-In-Time (按需注入)
```

### 1.2 三层分离

1. **配置层**: 定义哪些 Skill 可以被使用 (安全边界)
2. **加载层**: 启动时加载 Skill 元数据,但不注入内容
3. **激活层**: Agent 运行时决定何时激活,何时注入 Prompt

---

## 2. 完整流程 (工业级)

```
启动阶段 (Initialization)
├─ 加载 config.json
│  └─ 读取 agent.skillRoots (Allowlist)
├─ SkillManager.loadInternalStaticSkills()
│  └─ 加载所有 Skill 元数据 (name, description, tools, constraints)
├─ 组装初始 System Prompt
│  └─ 注入 Skill 发现列表 (仅名称和描述,无内容)
└─ Agent 启动完成

运行阶段 (Runtime)
├─ 用户请求: "扫描项目中所有 Python 文件,生成修改建议"
│
├─ Plan Phase
│  ├─ LLM 分析任务
│  ├─ LLM 决定需要 Skill: project_scan, code_modify
│  └─ 调用工具: skill_activate("project_scan")
│
├─ Skill 激活
│  ├─ SkillManager.activate("project_scan")
│  │  ├─ 检查 Skill 是否在 Allowlist
│  │  ├─ 检查 Skill 所需工具是否可用
│  │  ├─ 标记为激活状态
│  │  └─ 返回成功
│  │
│  └─ Agent 动态注入 Skill Prompt
│     ├─ 获取 SkillManager.getActiveSkillsPrompt()
│     ├─ 生成最小 Prompt:
│     │  ```
│     │  [SKILL ACTIVATED]
│     │  Skill: project_scan
│     │  Allowed tools:
│     │    - list_files
│     │    - search_text
│     │  Constraints:
│     │    - Only read specific files
│     │    - Do not overwrite
│     │  ```
│     └─ 注入到 messageHistory (作为 system 消息)
│
├─ Act Phase
│  └─ LLM 使用 Skill 的工具完成任务
│
├─ Observe Phase
│  └─ 任务完成
│
└─ 清理 (可选)
   └─ SkillManager.deactivate("project_scan")
```

---

## 3. 代码实现

### 3.1 SkillManager 核心 API

```cpp
class SkillManager {
public:
    struct Skill {
        std::string name;
        std::string description;
        std::string content;           // 完整 SKILL.md 内容
        
        // 从 SKILL.md 解析的元数据
        std::vector<std::string> requiredTools;    // 需要的工具
        std::vector<std::string> constraints;       // 约束条件
        std::string minimalInterface;              // 最小接口描述
    };
    
    // ========== 激活 API ==========
    
    /**
     * @brief 激活 Skill
     * @return 是否成功激活
     */
    bool activate(const std::string& name);
    
    /**
     * @brief 停用 Skill
     */
    void deactivate(const std::string& name);
    
    /**
     * @brief 停用所有 Skill
     */
    void deactivateAll();
    
    /**
     * @brief 检查 Skill 是否激活
     */
    bool isActive(const std::string& name) const;
    
    // ========== Prompt 生成 API ==========
    
    /**
     * @brief 获取 Skill 发现 Prompt (启动时注入)
     * 
     * 输出示例:
     * ```
     * # Available Skills (Lazy Loading Required)
     * - project_scan: Scan project files
     * - code_modify: Modify code files
     * 
     * ⚠️  Skills are NOT active by default. 
     * Call skill_activate(name) to use them.
     * ```
     */
    std::string getSkillDiscoveryPrompt() const;
    
    /**
     * @brief 获取激活 Skill 的 Prompt (运行时动态注入)
     * 
     * 输出示例:
     * ```
     * # ACTIVATED SKILLS
     * 
     * ## Skill: project_scan
     * **Description**: Scan project files
     * 
     * **Allowed Tools**:
     *   - list_files
     *   - search_text
     * 
     * **Constraints**:
     *   - Only read specific files
     *   - Do not overwrite
     * ```
     */
    std::string getActiveSkillsPrompt() const;

private:
    std::map<std::string, Skill> skills;      // 所有加载的 Skill (Allowlist)
    std::set<std::string> activeSkills;       // 当前激活的 Skill
};
```

### 3.2 AgentRuntime 集成

```cpp
class AgentRuntime {
public:
    AgentRuntime(
        std::shared_ptr<LLMClient> llmClient,
        ToolRegistry& toolRegistry,
        SkillManager* skillManager = nullptr
    );
    
    // Skill 管理
    bool activateSkill(const std::string& skillName);
    void deactivateSkill(const std::string& skillName);

private:
    void planPhase();
    std::string assembleSystemPrompt();
    
    SkillManager* skillMgr;
};
```

### 3.3 Prompt 注入时机

#### 3.3.1 启动时 (System Prompt)

```cpp
std::string AgentRuntime::assembleSystemPrompt() {
    std::ostringstream prompt;
    
    // ... 基础 Agent 指令 ...
    
    // 注入 Skill 发现列表
    if (skillMgr) {
        prompt << skillMgr->getSkillDiscoveryPrompt();
    }
    
    return prompt.str();
}
```

#### 3.3.2 运行时 (Dynamic Injection)

```cpp
void AgentRuntime::planPhase() {
    // 检查是否有新激活的 Skill
    if (skillMgr) {
        std::string activeSkillsPrompt = skillMgr->getActiveSkillsPrompt();
        
        if (!activeSkillsPrompt.empty()) {
            // 动态注入到消息历史
            nlohmann::json skillMsg;
            skillMsg["role"] = "system";
            skillMsg["content"] = activeSkillsPrompt;
            messageHistory.push_back(skillMsg);
        }
    }
    
    // 调用 LLM
    nlohmann::json response = llm->chatWithTools(messageHistory, tools);
    // ...
}
```

---

## 4. 工具集成

### 4.1 新增工具: skill_activate

```cpp
class SkillActivateTool : public ITool {
public:
    explicit SkillActivateTool(SkillManager* skillManager);
    
    std::string getName() const override { 
        return "skill_activate"; 
    }
    
    std::string getDescription() const override {
        return "Activate a specialized skill for current task. "
               "Once activated, the skill's tools and capabilities "
               "will become available.";
    }
    
    nlohmann::json getSchema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"skill_name", {
                    {"type", "string"},
                    {"description", "Name of the skill to activate"}
                }}
            }},
            {"required", {"skill_name"}}
        };
    }
    
    nlohmann::json execute(const nlohmann::json& args) override {
        std::string skillName = args["skill_name"];
        
        if (skillMgr->activate(skillName)) {
            return {
                {"success", true},
                {"message", "Skill activated: " + skillName},
                {"active_skills", skillMgr->getActiveSkills()}
            };
        } else {
            return {
                {"error", "Failed to activate skill: " + skillName}
            };
        }
    }

private:
    SkillManager* skillMgr;
};
```

### 4.2 新增工具: skill_deactivate

```cpp
class SkillDeactivateTool : public ITool {
    // 类似实现
};
```

---

## 5. 执行示例

### 5.1 完整执行流程

```
[User]: "扫描项目中所有 Python 文件,生成修改建议"

[Agent Iteration 1]
Phase: Planning
├─ LLM 收到任务
├─ LLM 分析: 需要 project_scan Skill
└─ LLM 规划:
   ├─ tool_call: skill_activate(skill_name="project_scan")
   └─ 等待激活完成

Phase: Acting
├─ 执行 skill_activate("project_scan")
├─ SkillManager.activate("project_scan")
│  ├─ 检查: project_scan 在 Allowlist ✓
│  ├─ 检查: 所需工具可用 ✓
│  └─ 标记为激活
└─ 返回: {"success": true}

Phase: Observing
└─ Skill 已激活,继续下一轮

[Agent Iteration 2]
Phase: Planning
├─ 动态注入 Skill Prompt:
│  ```
│  [SKILL ACTIVATED]
│  Skill: project_scan
│  Allowed tools: list_files, search_text
│  Constraints: Read-only access
│  ```
├─ LLM 现在可以看到 Skill 的能力
└─ LLM 规划:
   └─ tool_call: list_files(pattern="**/*.py")

Phase: Acting
├─ 执行 list_files
└─ 返回: ["src/main.py", "src/utils.py", ...]

Phase: Observing
└─ 获得文件列表,继续处理

[Agent Iteration 3]
Phase: Planning
├─ LLM 分析: 需要修改文件,激活 code_modify Skill
└─ LLM 规划:
   └─ tool_call: skill_activate(skill_name="code_modify")

... 循环继续 ...
```

---

## 6. 优势分析

### 6.1 vs 全量注入

| 方式 | Prompt 大小 | Token 成本 | 灵活性 |
|------|------------|----------|--------|
| **全量注入** | 所有 Skill × N KB | 高 (每次都付费) | 低 |
| **动态激活** | 仅激活 Skill | 低 (按需付费) | 高 |

### 6.2 关键优势

1. **Token 效率**: 只注入需要的 Skill
2. **安全性**: Allowlist 在配置层强制
3. **可观测性**: 明确知道哪些 Skill 被使用
4. **可扩展性**: 支持上百个 Skill 而不影响性能
5. **智能决策**: Agent 自主决定何时需要 Skill

---

## 7. 待实现清单

### 7.1 核心组件

- [x] SkillManager 激活 API (activate/deactivate)
- [x] SkillManager Prompt 生成 API
- [x] AgentRuntime Skill 集成
- [ ] SkillActivateTool 实现
- [ ] SkillDeactivateTool 实现
- [ ] Skill 元数据解析 (从 SKILL.md frontmatter)

### 7.2 高级功能 (可选)

- [ ] Skill 依赖检查 (Skill A 需要 Skill B)
- [ ] Skill 工具权限验证
- [ ] Skill 使用统计
- [ ] Skill 热重载 (开发模式)
- [ ] Skill 版本管理

---

## 8. Constitution 更新

需要在 `photon_agent_constitution_v_2.md` 中添加:

```markdown
## 5.3 Skill Activation Rules

- Skills MUST be explicitly activated before use
- Activation is performed via `skill_activate(name)` tool
- Only skills in the configuration allowlist can be activated
- Skill prompt injection happens AFTER activation, not at startup
- Multiple skills can be active simultaneously
- Skills should be deactivated when no longer needed (optional optimization)
```

---

## 9. 总结

**核心要点:**

1. **配置文件 = Allowlist**: 只定义允许哪些 Skill
2. **启动时 ≠ 激活时**: 启动时只加载元数据,不注入 Prompt
3. **激活 = 运行时决策**: Agent 在任务执行中决定何时激活
4. **Prompt 注入 = Just-In-Time**: 只在激活时动态注入最小内容
5. **工具驱动**: 通过 `skill_activate` 工具显式激活

这是工业级 Agent 系统的标准做法,确保:
- ✅ Token 效率
- ✅ 安全可控
- ✅ 可观测性
- ✅ 可扩展性
