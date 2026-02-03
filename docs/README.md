# Photon 文档中心

> Photon Agent 系统的完整文档索引

---

## 📚 文档目录

### 🚀 快速开始

- [主 README](../README.md) - 项目总览和快速开始
- [快速入门教程](./tutorials/SKILL_ACTIVATION_QUICK_REF.md) - 5 分钟了解核心概念

---

### 📖 核心文档

#### 架构设计

- [Constitution v2.0](../photon_agent_constitution_v_2.md) - Agent 行为规范和执行合约
- [设计文档](../design.md) - 系统架构和设计原则
- [重构总结](../REFACTOR_SUMMARY.md) - 架构重构历程

#### 实现细节

- [重构状态](../REFACTOR_STATUS.md) - 重构进度跟踪
- [重构完成报告](../REFACTOR_COMPLETE.md) - 完整重构记录
- [特性列表](../FEATURES.md) - 功能特性清单

---

### 📘 教程系列

完整教程列表请访问: [tutorials/README.md](./tutorials/README.md)

#### Skill 动态激活系列 ⭐ 推荐

1. [快速参考](./tutorials/SKILL_ACTIVATION_QUICK_REF.md) - 5 分钟入门
2. [完整总结](./tutorials/SKILL_ACTIVATION_SUMMARY.md) - 15 分钟理解原理
3. [设计文档](./tutorials/SKILL_ACTIVATION_DESIGN.md) - 30 分钟深入设计
4. [使用示例](./tutorials/SKILL_ACTIVATION_EXAMPLE.md) - 20 分钟实战场景
5. [实现清单](./tutorials/SKILL_ACTIVATION_TODO.md) - 10 分钟了解进度
6. [项目总览](./tutorials/SKILL_ACTIVATION_README.md) - 成果总结

**推荐阅读顺序**: 1 → 2 → 4 → 3 → 5

---

### 🔧 技术文档

#### 集成指南

- [LLM 适配器](./llm-adapters.md) - LLM 消息格式适配说明
- [MCP 集成](./mcp-integration.md) - 模型上下文协议集成

#### API 参考

- [工具 API](./api/tools.md) - 工具系统 API 文档
- [Skill API](./api/skills.md) - Skill 系统 API 文档
- [记忆 API](./api/memory.md) - 记忆系统 API 文档

---

## 🎯 按需求查找

### 我想快速了解 Photon
→ [主 README](../README.md)

### 我想理解 Skill 系统
→ [Skill 动态激活快速参考](./tutorials/SKILL_ACTIVATION_QUICK_REF.md)

### 我要开发新的 Skill
→ [Skill 设计文档](./tutorials/SKILL_ACTIVATION_DESIGN.md)

### 我要集成 Photon
→ [快速开始](../README.md#快速开始) + [配置说明](../config.json)

### 我要理解架构设计
→ [Constitution v2.0](../photon_agent_constitution_v_2.md) + [设计文档](../design.md)

---

## 📊 文档分类

### 按主题分类

#### 系统架构
- Constitution v2.0
- 设计文档
- 重构总结

#### Skill 系统
- Skill 动态激活系列 (6 篇)
- Skill API 参考

#### 工具系统
- 工具 API 参考
- MCP 集成指南

#### 记忆系统
- 记忆 API 参考
- 项目记忆设计

### 按难度分类

#### 入门级 (5-15 分钟)
- 主 README
- Skill 快速参考
- Skill 完整总结

#### 进阶级 (30-60 分钟)
- Constitution v2.0
- Skill 设计文档
- Skill 使用示例

#### 专家级 (1-2 小时)
- 设计文档
- 重构完成报告
- API 参考

---

## 🔍 常见问题

### Q: Skill 何时加载到 Prompt?
**A**: 运行时激活时才注入,不是启动时。详见 [Skill 完整总结](./tutorials/SKILL_ACTIVATION_SUMMARY.md)

### Q: 如何配置 Skill?
**A**: 在 `config.json` 中配置 `skill_roots`,详见 [Skill 快速参考](./tutorials/SKILL_ACTIVATION_QUICK_REF.md)

### Q: 如何写一个 Skill?
**A**: 创建 SKILL.md,包含 YAML frontmatter,详见 [Skill 设计文档](./tutorials/SKILL_ACTIVATION_DESIGN.md)

### Q: Photon 的核心优势是什么?
**A**: 原生性能 + 工程精度 + 智能管理,详见 [主 README](../README.md#为什么选择-photon)

### Q: 如何开始使用 Photon?
**A**: 查看 [快速开始](../README.md#快速开始)

---

## 📖 学习路径

### 新手路径 (30 分钟)

1. 阅读 [主 README](../README.md) (10 分钟)
2. 阅读 [Skill 快速参考](./tutorials/SKILL_ACTIVATION_QUICK_REF.md) (5 分钟)
3. 阅读 [Skill 完整总结](./tutorials/SKILL_ACTIVATION_SUMMARY.md) (15 分钟)

### 开发者路径 (2 小时)

1. 新手路径 (30 分钟)
2. 阅读 [Constitution v2.0](../photon_agent_constitution_v_2.md) (30 分钟)
3. 阅读 [Skill 设计文档](./tutorials/SKILL_ACTIVATION_DESIGN.md) (30 分钟)
4. 阅读 [Skill 使用示例](./tutorials/SKILL_ACTIVATION_EXAMPLE.md) (20 分钟)
5. 实践: 创建测试 Skill (10 分钟)

### 架构师路径 (4 小时)

1. 开发者路径 (2 小时)
2. 阅读 [设计文档](../design.md) (1 小时)
3. 阅读 [重构完成报告](../REFACTOR_COMPLETE.md) (1 小时)

---

## 🤝 贡献文档

发现文档问题?想要改进?

1. Fork 项目
2. 创建文档分支
3. 提交改进
4. 发起 Pull Request

### 文档规范

- 使用 Markdown 格式
- 保持简洁清晰
- 提供实际示例
- 更新文档索引

---

## 📞 获取帮助

- [GitHub Issues](https://github.com/QkHearn/Photon/issues)
- [教程索引](./tutorials/README.md)
- [常见问题](#常见问题)

---

<p align="center">
  <i>持续更新中...</i>
</p>

<p align="center">
  Made with 📖 by <a href="https://github.com/QkHearn">Hearn</a>
</p>
