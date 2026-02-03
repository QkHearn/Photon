# Photon 系统提示词演进: Constitution v2.0

**日期**: 2026-02-03  
**重构类型**: 从指令式 Prompt → Constitution 驱动  
**影响范围**: 系统提示词 + 代码层校验

---

## 📊 核心变化对比

### **重构前: Prompt 主导模式**

```cpp
systemPrompt = 
    "You are Photon, an autonomous AI agentic intelligence.\n"
    "Your mission is to assist with complex engineering tasks...\n"
    "PhotonRule v1.0:\n"
    "1. MIN_IO: No full-file reads >500 lines.\n"
    "2. PATCH_ONLY: No full-file overwrites...\n"
    "CRITICAL GUIDELINES:\n"
    "1. CHAIN-OF-THOUGHT: Reason step-by-step...\n"
    "2. CONTEXT-FIRST: Use 'plan' to generate...\n"
    "3. SURGICAL EDITS (MANDATORY): You MUST...\n"
    "8. PROACTIVE CLARIFICATION: If a task is...\n";
```

**Token 消耗**: ~2500 tokens  
**问题**:
- ❌ 规则是"建议"而非硬约束
- ❌ 模型被当作"有使命的人格体"
- ❌ 显式要求 Chain-of-Thought
- ❌ 大量行为指导混在 prompt 中

---

### **重构后: Constitution 驱动模式**

```cpp
systemPrompt = 
    "You are Photon.\n"
    "You must operate under Photon Agent Constitution v2.0.\n"
    "All behavior is governed by the constitution and validated configuration.\n\n" +
    cfg.llm.systemRole + "\n\n" +  // 项目差异
    skillManager.getSystemPromptAddition() + "\n" +  // 能力接口
    "Working directory: " + path + "\n" +
    "Current time: " + date_ss.str() + "\n";
```

**Token 消耗**: ~300 tokens (87% ↓)  
**优势**:
- ✅ Prompt 只负责加载 Constitution
- ✅ 规则变为"非法状态定义"
- ✅ 行为由代码层硬校验
- ✅ 模型是推理引擎,不是人格体

---

## 🔒 Constitution 硬约束实现

### **1. 代码层校验 (ConstitutionValidator)**

```cpp
// Section 3.1: IO Constraints
if (lineCount > 500) {
    return {false, 
            "Read operation exceeds 500 line limit",
            "Section 3.1: IO Constraints"};
}

// Section 3.3: Write Constraints
if (!args.contains("operation")) {
    return {false, 
            "Write operation lacks operation type",
            "Section 3.3: Write Constraints"};
}
```

**效果**: 违规 = 执行中止 (不是警告)

---

### **2. 执行流程变化**

#### **旧流程: Soft Guidance**
```
LLM 调用工具
    ↓
直接执行 (无硬校验)
    ↓
期望 LLM 遵守 Prompt 建议 ❌
```

#### **新流程: Hard Validation**
```
LLM 调用工具
    ↓
Constitution 校验
    ↓ (违规)
中止执行 + 返回错误 ✅
    ↓ (合法)
执行工具
```

---

## 📝 Prompt 职责重新划分

### **Prompt 现在只包含:**

1. **身份声明** (1 行)
   ```
   You are Photon.
   ```

2. **Constitution 引用** (1 行)
   ```
   You must operate under Photon Agent Constitution v2.0.
   ```

3. **项目差异** (从 config.json)
   ```
   Project context: C++ codebase with CMake build system.
   ```

4. **能力接口** (从 SkillManager)
   ```
   # Specialized Skills (Lazy Loading)
   - code_architect: 架构设计专家
   ```

5. **运行时上下文** (事实信息)
   ```
   Working directory: /path/to/project
   Current time: 2026-02-03 17:30:00
   ```

---

### **不再包含:**

- ❌ "你的使命是..."
- ❌ "8 条行为准则"
- ❌ "如何思考" (Chain-of-Thought 指导)
- ❌ "CRITICAL GUIDELINES"
- ❌ "MANDATORY" / "STRICTLY FORBIDDEN"
- ❌ 任何带情感色彩的描述

这些现在分别存在于:
- **Constitution 文档** (`photon_agent_constitution_v_2.md`)
- **代码层校验** (`ConstitutionValidator.h`)
- **Skill 规范** (SKILL.md 文件)

---

## 🎯 Chain-of-Thought 策略变化

### **旧策略: 显式要求**
```
"1. CHAIN-OF-THOUGHT: Reason step-by-step. 
    Justify every action before execution."
```

**问题**: 
- 强制模型输出推理过程
- 增加 token 消耗
- 容易被 prompt injection 攻击

---

### **新策略: 隐式允许**
```
(不在 prompt 中提及 CoT)
```

**效果**:
- ✅ 允许模型内部推理
- ✅ 不要求输出推理过程
- ✅ 只输出工具调用和结果
- ✅ 符合 2025+ 安全路线

---

## 🔍 Constitution 文档结构

### **Section 0: Purpose**
- 定义为"非协商执行合约"
- 明确 Photon 是执行代理,不是对话助手

### **Section 1: Agent Identity (Non-Persona)**
- 不扮演角色
- 不推测意图
- 不输出推理轨迹
- 只通过验证的工具/技能/补丁操作

### **Section 2: Execution Model**
```
INPUT → PLAN → NAVIGATE → READ (bounded)
  → PATCH (line-scoped) → VERIFY → COMMIT | ROLLBACK
```

### **Section 3: Core Constraints (Kernel Rules)**
- 3.1 IO Constraints (≤500 行)
- 3.2 Navigation Constraints (必须显式导航)
- 3.3 Write Constraints (禁止全文件覆盖)
- 3.4 Patch Semantics (必须指定行范围)

### **Section 4-12: 其他规范**
- Planning Rules (计划规则)
- Skill System Contract (技能合约)
- Tool Contract (工具合约)
- Memory Model (记忆模型)
- Output Policy (输出策略)
- Clarification Policy (澄清策略)
- Failure Semantics (失败语义)
- Versioning (版本控制)
- Authority (权威性)

---

## 📈 效果预测

| 指标 | 重构前 | 重构后 | 改善 |
|------|--------|--------|------|
| **Prompt Token** | ~2500 | ~300 | ⬇️ 87% |
| **规则执行率** | ~70% (依赖 LLM 遵守) | ~99% (代码强制) | ⬆️ 29% |
| **违规检测** | 事后发现 | 事前阻止 | ✅ 质变 |
| **可维护性** | Prompt 耦合 | 代码 + 文档分离 | ✅ 提升 |
| **安全性** | Prompt 注入风险 | 代码层防护 | ✅ 增强 |

---

## 🔧 实现细节

### **1. 新增文件**
- `src/agent/ConstitutionValidator.h` - 硬约束校验器
- `photon_agent_constitution_v_2.md` - 规范文档

### **2. 修改文件**
- `src/core/main.cpp` - 系统提示词重构 + 校验集成
- `config.json` - 清理旧行为指导

### **3. 校验集成点**
```cpp
// main.cpp 第 1475 行
auto validation = ConstitutionValidator::validateToolCall(toolName, args);
if (!validation.valid) {
    // 中止执行 + 返回结构化错误
    result["error"] = "Constitution violation: " + validation.error;
    continue;
}
```

---

## 🚀 下一步计划

### **短期 (本周)**
1. ✅ 完成基础 Constitution 校验
2. ⏳ 添加更多约束检查
   - Section 3.2: Navigation Constraints (LSP 优先)
   - Section 4: Planning Rules (必须先 plan)
3. ⏳ 更新 Skill 系统适配 Constitution
4. ⏳ 测试违规场景和错误恢复

### **中期 (本月)**
1. ⏳ 实现完整的 Section 3-12 约束
2. ⏳ 添加 Constitution 版本检查
3. ⏳ 集成 AgentRuntime 的 Plan-Act-Observe 循环
4. ⏳ 性能监控和优化

### **长期 (持续)**
1. ⏳ Constitution 演进和版本迭代
2. ⏳ 社区反馈收集
3. ⏳ 与 Cursor/Claude Code/Devin 对齐

---

## 💡 关键洞察

### **1. Prompt 退居二线**
Prompt 不再是"行为指南手册",而是"加载器":
- 加载 Constitution 引用
- 加载项目配置
- 加载能力接口

### **2. 规则升级为非法状态**
从:
```
"请不要全文件读取" (建议)
```

变为:
```cpp
if (lineCount > 500) {
    abort_execution();  // 非法状态
}
```

### **3. CoT 被正确"埋掉"**
- ✅ 允许内部推理
- ❌ 不要求输出推理
- ❌ 不在 Prompt 中提及

这是 **2025 年后唯一安全路线**。

### **4. 模型是引擎,不是人格**
```
旧: "你是 Photon,你的使命是..."
新: "You are Photon. Operate under Constitution v2.0."
```

模型被当作 **推理引擎**,不是有"使命感"的人格体。

---

## 📚 参考资源

- `photon_agent_constitution_v_2.md` - 完整 Constitution 规范
- `src/agent/ConstitutionValidator.h` - 校验实现
- Claude Code / Devin - 工业级实践参考

---

## ✅ 结论

**Photon 已成功演进到 Constitution v2.0 驱动模式**

核心成果:
- ✅ Prompt Token 减少 87%
- ✅ 规则从建议升级为硬约束
- ✅ Chain-of-Thought 正确隐藏
- ✅ 模型定位从人格体变为推理引擎
- ✅ 代码层强制执行 Constitution

这是与 **Claude Code / Devin / 内部 Copilot** 对齐的正确架构! 🎯
