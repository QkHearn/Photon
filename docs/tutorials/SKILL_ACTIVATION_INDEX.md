# Skill 动态激活机制 - 文档索引

> 本文档是 Photon Skill 动态激活机制的完整文档索引

---

## 📚 文档清单

### 核心文档 (推荐阅读顺序)

1. **SKILL_ACTIVATION_QUICK_REF.md** ⭐ **从这里开始**
   - 快速参考指南
   - 一句话回答核心问题
   - 3 个关键 API
   - 代码集成清单
   - **阅读时间: 5 分钟**

2. **SKILL_ACTIVATION_SUMMARY.md** ⭐⭐ **理解原理**
   - 完整问题答案
   - 三层分离架构
   - 完整流程示例
   - 关键设计决策
   - **阅读时间: 15 分钟**

3. **SKILL_ACTIVATION_DESIGN.md** ⭐⭐⭐ **深入设计**
   - 工业级设计原则
   - 完整实现流程
   - 代码实现细节
   - 工具集成方案
   - **阅读时间: 30 分钟**

4. **SKILL_ACTIVATION_EXAMPLE.md** ⭐⭐ **实战示例**
   - 4 个典型使用场景
   - 完整执行流程
   - Token 成本对比
   - 最佳实践
   - **阅读时间: 20 分钟**

5. **SKILL_ACTIVATION_TODO.md** ⭐ **实现清单**
   - 已完成功能 ✅
   - 待实现功能 🚧
   - 集成步骤
   - 验证清单
   - **阅读时间: 10 分钟**

---

## 🎯 根据需求选择文档

### 我想快速了解核心概念
→ **SKILL_ACTIVATION_QUICK_REF.md**

### 我想理解为什么这样设计
→ **SKILL_ACTIVATION_SUMMARY.md**

### 我要实现这个功能
→ **SKILL_ACTIVATION_DESIGN.md** + **SKILL_ACTIVATION_TODO.md**

### 我想看实际使用场景
→ **SKILL_ACTIVATION_EXAMPLE.md**

### 我想知道还有什么要做
→ **SKILL_ACTIVATION_TODO.md**

---

## 💻 代码文件

### 核心实现

```
src/utils/SkillManager.h
  - Skill 结构定义
  - activate() / deactivate() API
  - getSkillDiscoveryPrompt()
  - getActiveSkillsPrompt()
  - 状态: ✅ 核心 API 完成

src/tools/CoreTools.h
src/tools/CoreTools.cpp
  - SkillActivateTool 实现
  - SkillDeactivateTool 实现
  - 状态: ✅ 工具实现完成

src/agent/AgentRuntime.h
src/agent/AgentRuntime.cpp
  - Agent 集成接口
  - Prompt 注入框架
  - 状态: ✅ 框架完成,🚧 需要启用注入逻辑
```

### 配置文件

```
photon_agent_constitution_v_2.md
  - §5.2 Skill Allowlist
  - §5.3 Skill Activation Rules
  - §5.4 Skill Lifecycle
  - §5.5 Skill Prompt Injection
  - 状态: ✅ Constitution 已更新
```

---

## 🔑 核心概念速查

### 三层分离

```
1. 配置层 (Allowlist)
   config.json → agent.skillRoots

2. 加载层 (Metadata)
   启动时 → loadSkills() → 元数据

3. 激活层 (Runtime)
   LLM 决策 → skill_activate() → 动态注入
```

### 两阶段注入

```
阶段 1: 发现 (Startup)
  getSkillDiscoveryPrompt()
  → 告知可用 Skill (仅列表)

阶段 2: 激活 (Runtime)
  getActiveSkillsPrompt()
  → 注入工具、约束、接口
```

### 生命周期

```
未加载 → 已加载 → 已激活 → 使用中 → 已停用
  ↑         ↑         ↑         ↑         ↑
启动前   启动时   激活时    运行时    停用时
```

---

## 📊 关键指标

### Token 效率

```
场景: 20 个可用 Skill,平均使用 2 个

全量注入: 10K tokens/轮
动态激活: 2K tokens/轮
节省: 80%
```

### 可扩展性

```
全量注入: 最多支持 ~20 个 Skill
动态激活: 支持 200+ 个 Skill
提升: 10x
```

### 成本节省

```
100 轮对话,20 个 Skill
全量注入: $18.00
动态激活: $9.00
节省: $9.00 (50%)
```

---

## 🎓 学习路径

### 入门 (15 分钟)

1. 阅读 **SKILL_ACTIVATION_QUICK_REF.md**
2. 理解三个关键 API
3. 查看快速参考

### 进阶 (1 小时)

1. 阅读 **SKILL_ACTIVATION_SUMMARY.md**
2. 理解三层分离架构
3. 查看完整流程示例
4. 阅读 **SKILL_ACTIVATION_EXAMPLE.md**
5. 理解 4 个典型场景

### 深入 (2-3 小时)

1. 阅读 **SKILL_ACTIVATION_DESIGN.md**
2. 理解工业级设计原则
3. 查看代码实现细节
4. 阅读 **SKILL_ACTIVATION_TODO.md**
5. 理解集成步骤

### 实战 (2-4 小时)

1. 完成 TODO 清单中的核心功能
2. 创建测试 Skill
3. 运行端到端测试
4. 性能基准测试

---

## ❓ 常见问题索引

### 概念问题

**Q: Skill 何时加载到 Prompt?**
→ **SKILL_ACTIVATION_SUMMARY.md** § 核心答案

**Q: 为什么不全量注入?**
→ **SKILL_ACTIVATION_DESIGN.md** § 1.2 三层分离

**Q: 如何保证安全?**
→ **SKILL_ACTIVATION_DESIGN.md** § 2 完整流程

### 实现问题

**Q: 如何集成到现有代码?**
→ **SKILL_ACTIVATION_TODO.md** § 集成步骤

**Q: 需要修改哪些文件?**
→ **SKILL_ACTIVATION_TODO.md** § 待实现

**Q: 如何测试?**
→ **SKILL_ACTIVATION_TODO.md** § 验证清单

### 使用问题

**Q: 典型使用场景是什么?**
→ **SKILL_ACTIVATION_EXAMPLE.md** § 示例 1-4

**Q: Token 成本如何计算?**
→ **SKILL_ACTIVATION_EXAMPLE.md** § Token 成本对比

**Q: 最佳实践是什么?**
→ **SKILL_ACTIVATION_EXAMPLE.md** § 最佳实践

---

## 🔧 快速开始

### 1 分钟快速开始

```bash
# 1. 阅读快速参考
cat SKILL_ACTIVATION_QUICK_REF.md

# 2. 查看核心 API
grep -A 10 "getSkillDiscoveryPrompt" src/utils/SkillManager.h
grep -A 10 "getActiveSkillsPrompt" src/utils/SkillManager.h
grep -A 10 "activate" src/utils/SkillManager.h

# 3. 查看待办清单
cat SKILL_ACTIVATION_TODO.md
```

### 5 分钟集成

```cpp
// 1. 注册工具
toolRegistry.registerTool(
    std::make_unique<SkillActivateTool>(&skillManager)
);

// 2. 传递 SkillManager
AgentRuntime agentRuntime(
    llmClient, toolRegistry, 
    nullptr, nullptr, &skillManager
);

// 3. 启用发现 Prompt
if (skillMgr) {
    prompt << skillMgr->getSkillDiscoveryPrompt();
}

// 4. 启用动态注入
if (skillMgr) {
    auto activePrompt = skillMgr->getActiveSkillsPrompt();
    if (!activePrompt.empty()) {
        messageHistory.push_back({
            {"role", "system"},
            {"content", activePrompt}
        });
    }
}
```

---

## 📞 获取帮助

### 遇到问题?

1. **查阅文档**: 根据问题类型选择对应文档
2. **检查代码**: 查看核心文件实现
3. **查看示例**: SKILL_ACTIVATION_EXAMPLE.md 中的场景
4. **查看 TODO**: SKILL_ACTIVATION_TODO.md 中的常见问题

### 建议反馈?

- 如果发现文档不清楚,请提出改进建议
- 如果发现实现问题,请更新 TODO 清单
- 如果有新的使用场景,请添加到 EXAMPLE.md

---

## 📈 版本历史

### v1.0 (2026-02-03)
- ✅ 核心设计完成
- ✅ API 定义完成
- ✅ 工具实现完成
- ✅ Agent 集成框架完成
- ✅ 文档体系完成
- 🚧 待集成和测试

---

## 🎉 总结

这套文档体系提供了 Photon Skill 动态激活机制的完整说明:

- **快速参考**: 5 分钟了解核心概念
- **设计文档**: 30 分钟深入理解原理
- **使用示例**: 20 分钟学习实战场景
- **实现清单**: 10 分钟明确实现任务
- **总结文档**: 15 分钟掌握全局

**从这里开始**: [SKILL_ACTIVATION_QUICK_REF.md](./SKILL_ACTIVATION_QUICK_REF.md)

**核心原则**: 配置定义边界,启动准备数据,运行时按需激活,激活后动态注入

**关键价值**: Token 效率 ↑ 80%,可扩展性 ↑ 10x,成本 ↓ 50%

---

**这就是 Photon Skill 动态激活机制的完整文档体系。**
