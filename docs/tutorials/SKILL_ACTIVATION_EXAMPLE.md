# Photon Skill 动态激活 - 使用示例

> 本文档展示 Skill 动态激活机制的实际使用场景

---

## 示例 1: 项目扫描任务

### 用户请求

```
扫描项目中所有 Python 文件,生成修改建议
```

### Agent 执行流程

#### Iteration 1: Skill 发现

```
[Agent] Planning...
[LLM] 分析任务:
  - 需要扫描项目文件
  - 检测到需要 "project_scan" Skill
  
[LLM] 计划:
  1. 激活 project_scan Skill
  2. 使用 Skill 提供的工具扫描文件
  
[LLM] Tool Calls:
  - skill_activate(skill_name="project_scan")

[Agent] Acting...
[Agent] 执行 skill_activate("project_scan")

[SkillManager] Activated skill: project_scan

[Tool Result]:
{
  "success": true,
  "message": "Skill activated: project_scan",
  "active_skills": ["project_scan"],
  "skill_prompt": "
    # ACTIVATED SKILLS
    
    ## Skill: project_scan
    **Description**: Scan and analyze project structure
    
    **Allowed Tools**:
      - list_project_files
      - read_code_block
    
    **Constraints**:
      - Read-only access
      - Do not modify files
      - Maximum 500 lines per read
  "
}

[Agent] Observing...
[Agent] Skill activated successfully, continue to next iteration
```

#### Iteration 2: 使用 Skill

```
[Agent] Planning...
[LLM] 现在可以看到 project_scan Skill 的 Prompt
[LLM] 可用工具: list_project_files, read_code_block

[LLM] 计划:
  1. 列出所有 Python 文件
  
[LLM] Tool Calls:
  - list_project_files(pattern="**/*.py")

[Agent] Acting...
[Agent] 执行 list_project_files

[Tool Result]:
{
  "files": [
    "src/main.py",
    "src/utils.py",
    "src/config.py",
    "tests/test_main.py"
  ]
}

[Agent] Observing...
[Agent] 获得文件列表,继续分析
```

#### Iteration 3: 读取文件

```
[Agent] Planning...
[LLM] 计划: 依次读取每个文件

[LLM] Tool Calls:
  - read_code_block(file_path="src/main.py")
  - read_code_block(file_path="src/utils.py")

[Agent] Acting...
[Agent] 执行 read_code_block (批量)

[Tool Results]:
{
  "src/main.py": {
    "lines": 120,
    "content": "..."
  },
  "src/utils.py": {
    "lines": 85,
    "content": "..."
  }
}

[Agent] Observing...
[Agent] 分析代码,准备生成修改建议
```

#### Iteration 4: 激活代码修改 Skill

```
[Agent] Planning...
[LLM] 需要修改文件,激活 code_modify Skill

[LLM] Tool Calls:
  - skill_activate(skill_name="code_modify")

[Agent] Acting...
[SkillManager] Activated skill: code_modify

[Tool Result]:
{
  "success": true,
  "active_skills": ["project_scan", "code_modify"],
  "skill_prompt": "
    ## Skill: code_modify
    **Allowed Tools**:
      - apply_patch
    
    **Constraints**:
      - Line-level edits only
      - No full-file overwrites
      - Create backup before modification
  "
}

[Agent] Observing...
[Agent] 两个 Skill 现在同时激活
```

#### Iteration 5: 应用修改

```
[Agent] Planning...
[LLM] 可以使用 apply_patch 工具

[LLM] Tool Calls:
  - apply_patch(
      file_path="src/main.py",
      operation="replace",
      start_line=10,
      end_line=15,
      content="# Improved implementation\n..."
    )

[Agent] Acting...
[Agent] 执行 apply_patch

[Tool Result]:
{
  "success": true,
  "backup_created": "/tmp/backup_main.py",
  "lines_modified": 5
}

[Agent] Observing...
[Agent] 修改完成,任务结束
```

#### Iteration 6: 清理 (可选)

```
[Agent] Planning...
[LLM] 任务完成,释放 Skill

[LLM] Tool Calls:
  - skill_deactivate(skill_name="project_scan")
  - skill_deactivate(skill_name="code_modify")

[Agent] Acting...
[SkillManager] Deactivated all skills

[Agent] Task completed.
```

---

## 示例 2: 复杂重构任务

### 用户请求

```
将所有使用 MySQL 的代码迁移到 PostgreSQL
```

### Agent 执行流程

#### Phase 1: 分析阶段

```
Iteration 1:
  - 激活 project_scan Skill
  - 搜索所有 MySQL 相关代码

Iteration 2-5:
  - 使用 project_scan 工具分析依赖
  - 识别所有需要修改的文件
```

#### Phase 2: 计划阶段

```
Iteration 6:
  - 激活 migration_planner Skill (假设存在)
  - 生成迁移计划
  - 识别风险点
```

#### Phase 3: 执行阶段

```
Iteration 7-20:
  - 激活 code_modify Skill
  - 逐文件应用修改
  - 使用 apply_patch 工具精确修改
```

#### Phase 4: 验证阶段

```
Iteration 21-25:
  - 激活 test_runner Skill (假设存在)
  - 运行测试
  - 验证迁移结果
```

---

## 示例 3: Skill 依赖链

### 场景: 高级代码审查

```
用户: "执行全面代码审查,包括安全性、性能、可维护性"

Agent 激活顺序:
1. code_review Skill (主 Skill)
   ↓
2. security_scan Skill (检测安全漏洞)
   ↓
3. performance_profiler Skill (性能分析)
   ↓
4. code_quality Skill (代码质量检查)

最终状态:
  active_skills: [
    "code_review",
    "security_scan", 
    "performance_profiler",
    "code_quality"
  ]
  
Prompt 大小:
  - 初始: 基础 Agent Prompt (2K tokens)
  - 激活 4 个 Skill 后: +4K tokens
  - 总计: 6K tokens
  
vs 全量注入:
  - 如果有 20 个 Skill: 20K tokens
  - 节省: 14K tokens = 70% 成本降低
```

---

## 示例 4: 动态 Skill 切换

### 场景: 多阶段任务

```
任务: "实现新功能 + 编写测试 + 更新文档"

阶段 1: 实现功能
├─ 激活: feature_development Skill
├─ 使用工具: code_modify, apply_patch
└─ 完成后: 停用 feature_development

阶段 2: 编写测试
├─ 激活: test_generation Skill
├─ 使用工具: generate_test, run_test
└─ 完成后: 停用 test_generation

阶段 3: 更新文档
├─ 激活: documentation Skill
├─ 使用工具: update_readme, generate_docs
└─ 完成后: 停用 documentation

优势:
- 每个阶段只保持 1 个 Skill 激活
- Prompt 大小始终保持在最小
- 上下文清晰,不会混淆不同阶段的约束
```

---

## Token 成本对比

### 场景: 100 轮对话,20 个可用 Skill

#### 方式 1: 全量注入

```
每个 Skill Prompt: 500 tokens
总 Skill Prompt: 20 × 500 = 10,000 tokens

每轮对话:
  - Input: 基础 Prompt (2K) + Skill Prompt (10K) = 12K tokens
  - 100 轮 Input: 1,200,000 tokens

成本 (假设 $0.01/1K tokens):
  - Input: $12.00
  - Output: ~$6.00
  - 总计: $18.00
```

#### 方式 2: 动态激活

```
平均每轮激活 Skill: 2 个
每个 Skill Prompt: 500 tokens

每轮对话:
  - Input: 基础 Prompt (2K) + Skill Prompt (1K) = 3K tokens
  - 100 轮 Input: 300,000 tokens

成本:
  - Input: $3.00
  - Output: ~$6.00
  - 总计: $9.00

节省: $9.00 (50% 成本降低)
```

### 极端场景: 企业级系统 (200 个 Skill)

```
全量注入:
  - Skill Prompt: 200 × 500 = 100K tokens
  - 每轮对话: 102K tokens
  - 不可行 (超过大部分模型的上下文限制)

动态激活:
  - 平均激活: 3-5 个 Skill
  - 每轮对话: 2K + 2.5K = 4.5K tokens
  - 完全可行
  
结论: 动态激活是唯一可扩展的方案
```

---

## 最佳实践

### 1. Skill 激活时机

✅ **正确做法**:
```
任务: "修复 bug"
→ 先分析问题 (无需 Skill)
→ 确定需要修改代码 → 激活 code_modify
→ 需要运行测试 → 激活 test_runner
```

❌ **错误做法**:
```
任务: "修复 bug"
→ 立即激活所有可能用到的 Skill
→ 浪费 tokens
```

### 2. Skill 停用时机

✅ **推荐** (优化成本):
```
任务完成后立即停用
```

⚠️ **也可接受** (简化逻辑):
```
任务结束时统一清理
```

### 3. Skill 粒度设计

✅ **好的 Skill 设计**:
```
- project_scan: 扫描项目结构
- code_modify: 修改代码
- test_runner: 运行测试
```

❌ **不好的 Skill 设计**:
```
- do_everything: 包含所有功能 (无法按需激活)
- read_one_file: 太细粒度 (激活开销大)
```

---

## 总结

### 核心优势

1. **成本效率**: 按需加载,节省 50-90% Token 成本
2. **可扩展性**: 支持数百个 Skill 而不影响性能
3. **上下文清晰**: 每个阶段只显示相关能力
4. **安全可控**: Allowlist 强制,激活可审计
5. **智能决策**: Agent 自主判断何时需要 Skill

### 关键设计

```
配置层 (config.json)
  ↓ 定义 Allowlist
加载层 (Startup)
  ↓ 加载元数据
发现层 (Initial Prompt)
  ↓ 告知可用 Skill
激活层 (Runtime)
  ↓ 按需激活
注入层 (Dynamic Prompt)
  ↓ Just-In-Time 注入
```

这是工业级 Agent 系统的标准架构。
