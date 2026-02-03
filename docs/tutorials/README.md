# Photon 教程文档

> Photon Agent 系统的完整学习指南

---

## 📚 教程列表

### 🚀 快速入门

**推荐阅读顺序**:

1. [**Skill 动态激活 - 快速参考**](./SKILL_ACTIVATION_QUICK_REF.md) ⭐ 5 分钟
   - 最快了解核心概念
   - 3 个关键 API
   - 代码集成清单

2. [**Skill 动态激活 - 完整总结**](./SKILL_ACTIVATION_SUMMARY.md) ⭐⭐ 15 分钟
   - 完整问题答案
   - 三层分离架构
   - 关键设计决策

---

### 📖 深入学习

3. [**Skill 动态激活 - 设计文档**](./SKILL_ACTIVATION_DESIGN.md) ⭐⭐⭐ 30 分钟
   - 工业级设计原则
   - 完整实现流程
   - 代码实现细节

4. [**Skill 动态激活 - 使用示例**](./SKILL_ACTIVATION_EXAMPLE.md) ⭐⭐ 20 分钟
   - 4 个典型使用场景
   - 完整执行流程
   - Token 成本对比

---

### 🔧 实践指南

5. [**Skill 动态激活 - 实现清单**](./SKILL_ACTIVATION_TODO.md) ⭐ 10 分钟
   - 已完成功能清单
   - 待实现功能清单
   - 集成步骤详解

6. [**Skill 动态激活 - 项目总览**](./SKILL_ACTIVATION_README.md) ⭐ 10 分钟
   - 项目成果总览
   - 核心价值说明
   - 下一步指引

---

## 🎯 按需求选择

### 我想快速了解 Skill 系统
→ [快速参考](./SKILL_ACTIVATION_QUICK_REF.md)

### 我想理解为什么这样设计
→ [完整总结](./SKILL_ACTIVATION_SUMMARY.md)

### 我要实现或集成这个功能
→ [设计文档](./SKILL_ACTIVATION_DESIGN.md) + [实现清单](./SKILL_ACTIVATION_TODO.md)

### 我想看实际使用场景
→ [使用示例](./SKILL_ACTIVATION_EXAMPLE.md)

---

## 💡 核心概念

### Skill 动态激活机制

**核心问题**: "配置文件允许了 Skill,但什么时候才真正加载/注入 Prompt 里?"

**核心答案**: **运行时按需激活,Just-In-Time 注入**

### 三层架构

```
配置层 (config.json)
  ↓ 定义 Allowlist
加载层 (启动时)
  ↓ 加载元数据
激活层 (运行时)
  ↓ 按需激活
注入层 (动态)
  ↓ Just-In-Time 注入
```

### 核心价值

- ✅ **Token 效率**: 节省 80-97% Token 成本
- ✅ **可扩展性**: 支持 200+ Skill (vs 全量注入 ~20 个)
- ✅ **智能决策**: LLM 自主按需激活
- ✅ **安全可控**: Allowlist + Runtime 双重验证

---

## 📊 学习路径

### 入门 (15 分钟)

1. 阅读 [快速参考](./SKILL_ACTIVATION_QUICK_REF.md)
2. 理解三个关键 API
3. 查看快速示例

### 进阶 (1 小时)

1. 阅读 [完整总结](./SKILL_ACTIVATION_SUMMARY.md)
2. 理解三层分离架构
3. 查看完整流程
4. 阅读 [使用示例](./SKILL_ACTIVATION_EXAMPLE.md)

### 深入 (2-3 小时)

1. 阅读 [设计文档](./SKILL_ACTIVATION_DESIGN.md)
2. 理解工业级设计原则
3. 查看代码实现细节
4. 阅读 [实现清单](./SKILL_ACTIVATION_TODO.md)

### 实战 (2-4 小时)

1. 完成集成任务
2. 创建测试 Skill
3. 运行端到端测试
4. 性能基准测试

---

## 🔑 关键文档速查

| 文档 | 用途 | 阅读时间 | 推荐度 |
|------|------|---------|--------|
| [快速参考](./SKILL_ACTIVATION_QUICK_REF.md) | 快速了解核心概念 | 5 分钟 | ⭐⭐⭐⭐⭐ |
| [完整总结](./SKILL_ACTIVATION_SUMMARY.md) | 理解设计原理 | 15 分钟 | ⭐⭐⭐⭐ |
| [设计文档](./SKILL_ACTIVATION_DESIGN.md) | 深入实现细节 | 30 分钟 | ⭐⭐⭐ |
| [使用示例](./SKILL_ACTIVATION_EXAMPLE.md) | 学习实战场景 | 20 分钟 | ⭐⭐⭐⭐ |
| [实现清单](./SKILL_ACTIVATION_TODO.md) | 了解实现状态 | 10 分钟 | ⭐⭐⭐ |
| [项目总览](./SKILL_ACTIVATION_README.md) | 查看项目成果 | 10 分钟 | ⭐⭐ |

---

## 📖 相关文档

- [Photon 主 README](../../README.md)
- [Constitution v2.0](../../photon_agent_constitution_v_2.md)
- [重构总结](../../REFACTOR_SUMMARY.md)

---

## ❓ 常见问题

### Q: Skill 何时加载到 Prompt?
A: 运行时激活时才注入,不是启动时。详见 [完整总结](./SKILL_ACTIVATION_SUMMARY.md)

### Q: 如何配置 Skill?
A: 在 `config.json` 中配置 `skill_roots`,详见 [快速参考](./SKILL_ACTIVATION_QUICK_REF.md)

### Q: 如何写一个 Skill?
A: 创建 SKILL.md,包含 YAML frontmatter,详见 [设计文档](./SKILL_ACTIVATION_DESIGN.md)

### Q: LLM 如何知道有哪些 Skill?
A: 启动时注入发现列表,激活后注入详细信息,详见 [完整总结](./SKILL_ACTIVATION_SUMMARY.md)

---

**开始学习**: [SKILL_ACTIVATION_QUICK_REF.md](./SKILL_ACTIVATION_QUICK_REF.md)
