# Photon 教程文档

> Photon Agent 系统的完整学习指南

---

## 📚 教程列表

### 🚀 快速入门

**推荐阅读顺序**:

#### Skill 动态激活系统

1. [**Skill 动态激活 - 快速参考**](./SKILL_ACTIVATION_QUICK_REF.md) ⭐ 5 分钟
   - 最快了解核心概念
   - 3 个关键 API
   - 代码集成清单

2. [**Skill 动态激活 - 完整总结**](./SKILL_ACTIVATION_SUMMARY.md) ⭐⭐ 15 分钟
   - 完整问题答案
   - 三层分离架构
   - 关键设计决策

#### Agent 智能分析能力

3. [**符号搜索快速入门**](./QUICK_START_AST.md) ⭐ 5 分钟 🆕
   - AST 符号分析能力
   - 智能文件读取优化
   - 使用方法和示例

4. [**语义搜索快速入门**](./QUICK_START_SEMANTIC_SEARCH.md) ⭐ 5 分钟 🆕
   - 语义搜索内部能力
   - 模糊查询自动定位
   - 零配置启用方法

---

### 📖 深入学习

#### Skill 系统

5. [**Skill 动态激活 - 设计文档**](./SKILL_ACTIVATION_DESIGN.md) ⭐⭐⭐ 30 分钟
   - 工业级设计原则
   - 完整实现流程
   - 代码实现细节

6. [**Skill 动态激活 - 使用示例**](./SKILL_ACTIVATION_EXAMPLE.md) ⭐⭐ 20 分钟
   - 4 个典型使用场景
   - 完整执行流程
   - Token 成本对比

#### Agent 内部能力

7. [**符号搜索集成详解**](./AGENT_AST_ANALYSIS.md) ⭐⭐⭐ 25 分钟 🆕
   - AST 分析架构设计
   - 拦截与增强机制
   - 符号提取实现细节

8. [**语义搜索集成详解**](./SEMANTIC_SEARCH_INTEGRATION.md) ⭐⭐⭐ 30 分钟 🆕
   - 语义搜索工作原理
   - Agent 内部能力模式
   - 完整集成流程

---

### 🔧 实践指南

9. [**Skill 动态激活 - 实现清单**](./SKILL_ACTIVATION_TODO.md) ⭐ 10 分钟
   - 已完成功能清单
   - 待实现功能清单
   - 集成步骤详解

10. [**Skill 动态激活 - 项目总览**](./SKILL_ACTIVATION_README.md) ⭐ 10 分钟
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

### 我想了解 Agent 的智能分析能力 🆕
→ [符号搜索快速入门](./QUICK_START_AST.md) + [语义搜索快速入门](./QUICK_START_SEMANTIC_SEARCH.md)

### 我想深入理解符号/语义搜索机制 🆕
→ [符号搜索详解](./AGENT_AST_ANALYSIS.md) + [语义搜索详解](./SEMANTIC_SEARCH_INTEGRATION.md)

---

## 💡 核心概念

### 1. Skill 动态激活机制

**核心问题**: "配置文件允许了 Skill,但什么时候才真正加载/注入 Prompt 里?"

**核心答案**: **运行时按需激活,Just-In-Time 注入**

#### 三层架构

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

#### 核心价值

- ✅ **Token 效率**: 节省 80-97% Token 成本
- ✅ **可扩展性**: 支持 200+ Skill (vs 全量注入 ~20 个)
- ✅ **智能决策**: LLM 自主按需激活
- ✅ **安全可控**: Allowlist + Runtime 双重验证

---

### 2. Agent 内部能力机制 🆕

**核心问题**: "LLM 如何从模糊查询找到精确代码?"

**核心答案**: **Agent 内部能力自动增强,对 LLM 透明**

#### 双层搜索架构

```
用户查询 "找到登录相关代码"
    ↓
LLM 生成工具调用（可能不准确）
    ↓
Agent 拦截与分析
    ├─ 符号搜索 (SymbolManager)  ← 结构化分析 (快速)
    └─ 语义搜索 (SemanticManager) ← 模糊匹配 (智能)
    ↓
Agent 注入增强信息
    ↓
LLM 基于增强上下文做决策
    ↓
精确工具调用
```

#### 核心价值

- ✅ **智能增强**: Agent 自动判断何时使用分析能力
- ✅ **透明操作**: LLM 无需知道内部能力,只看到增强结果
- ✅ **渐进查询**: 粗定位（语义）→ 精确读取（工具）
- ✅ **多层结合**: 符号分析 + 语义搜索 = 强大代码理解

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

### Skill 动态激活系统

| 文档 | 用途 | 阅读时间 | 推荐度 |
|------|------|---------|--------|
| [快速参考](./SKILL_ACTIVATION_QUICK_REF.md) | 快速了解核心概念 | 5 分钟 | ⭐⭐⭐⭐⭐ |
| [完整总结](./SKILL_ACTIVATION_SUMMARY.md) | 理解设计原理 | 15 分钟 | ⭐⭐⭐⭐ |
| [设计文档](./SKILL_ACTIVATION_DESIGN.md) | 深入实现细节 | 30 分钟 | ⭐⭐⭐ |
| [使用示例](./SKILL_ACTIVATION_EXAMPLE.md) | 学习实战场景 | 20 分钟 | ⭐⭐⭐⭐ |
| [实现清单](./SKILL_ACTIVATION_TODO.md) | 了解实现状态 | 10 分钟 | ⭐⭐⭐ |
| [项目总览](./SKILL_ACTIVATION_README.md) | 查看项目成果 | 10 分钟 | ⭐⭐ |

### Agent 智能分析能力 🆕

| 文档 | 用途 | 阅读时间 | 推荐度 |
|------|------|---------|--------|
| [符号搜索快速入门](./QUICK_START_AST.md) | 5 分钟了解 AST 能力 | 5 分钟 | ⭐⭐⭐⭐⭐ |
| [语义搜索快速入门](./QUICK_START_SEMANTIC_SEARCH.md) | 5 分钟了解语义搜索 | 5 分钟 | ⭐⭐⭐⭐⭐ |
| [符号搜索详解](./AGENT_AST_ANALYSIS.md) | 深入理解符号分析 | 25 分钟 | ⭐⭐⭐⭐ |
| [语义搜索详解](./SEMANTIC_SEARCH_INTEGRATION.md) | 深入理解语义搜索 | 30 分钟 | ⭐⭐⭐⭐ |

---

## 📖 相关文档

- [Photon 主 README](../../README.md)
- [Constitution v2.0](../../photon_agent_constitution_v_2.md)
- [重构总结](../../REFACTOR_SUMMARY.md)

---

## ❓ 常见问题

### Skill 系统

**Q: Skill 何时加载到 Prompt?**
A: 运行时激活时才注入,不是启动时。详见 [完整总结](./SKILL_ACTIVATION_SUMMARY.md)

**Q: 如何配置 Skill?**
A: 在 `config.json` 中配置 `skill_roots`,详见 [快速参考](./SKILL_ACTIVATION_QUICK_REF.md)

**Q: 如何写一个 Skill?**
A: 创建 SKILL.md,包含 YAML frontmatter,详见 [设计文档](./SKILL_ACTIVATION_DESIGN.md)

**Q: LLM 如何知道有哪些 Skill?**
A: 启动时注入发现列表,激活后注入详细信息,详见 [完整总结](./SKILL_ACTIVATION_SUMMARY.md)

### Agent 智能分析 🆕

**Q: 符号搜索和语义搜索有什么区别?**
A: 符号搜索基于 AST 结构化分析(精确快速),语义搜索基于语义相似度(模糊智能)。详见 [符号搜索](./QUICK_START_AST.md) 和 [语义搜索](./QUICK_START_SEMANTIC_SEARCH.md)

**Q: 语义搜索何时触发?**
A: Agent 检测到模糊查询时自动触发(如路径包含空格/中文/疑问词)。详见 [语义搜索详解](./SEMANTIC_SEARCH_INTEGRATION.md)

**Q: 为什么叫"内部能力"?**
A: LLM 无需知道这些能力存在,Agent 在后台自动使用并注入增强信息。详见 [符号搜索详解](./AGENT_AST_ANALYSIS.md)

**Q: 如何启用这些能力?**
A: 创建 AgentRuntime 时传入对应的 Manager 即可,零额外配置。详见快速入门文档

---

## 🚀 开始学习

### 快速通道 (5 分钟入门)

1. **Skill 系统**: [SKILL_ACTIVATION_QUICK_REF.md](./SKILL_ACTIVATION_QUICK_REF.md)
2. **符号搜索**: [QUICK_START_AST.md](./QUICK_START_AST.md) 🆕
3. **语义搜索**: [QUICK_START_SEMANTIC_SEARCH.md](./QUICK_START_SEMANTIC_SEARCH.md) 🆕

### 深入学习路径

**路径 1: Skill 动态激活系统** (1-2 小时)
```
快速参考 → 完整总结 → 使用示例 → 设计文档
```

**路径 2: Agent 智能分析能力** (1 小时) 🆕
```
符号搜索快速入门 → 语义搜索快速入门 → 详解文档
```

**路径 3: 完整系统理解** (3-4 小时)
```
所有快速入门 → 所有详解文档 → 实践指南
```

---

**推荐起点**: 
- 想了解 Skill 系统 → [快速参考](./SKILL_ACTIVATION_QUICK_REF.md)
- 想了解智能分析 → [符号搜索](./QUICK_START_AST.md) + [语义搜索](./QUICK_START_SEMANTIC_SEARCH.md) 🆕
