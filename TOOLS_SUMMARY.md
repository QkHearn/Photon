# Photon 工具列表

Photon 拥有分层工具架构，包含**核心工具(Core Tools)**和**MCP工具**两大类。

## 🛠️ 核心工具 (Core Tools)

这些是最基础的原子工具，在 `src/tools/CoreTools.h` 中定义，启动时注册到 Agent 供 LLM 调用：

| 工具名 | 一句话 |
|--------|--------|
| **read_code_block** | 按路径/符号/行范围读代码，无参数时返回符号摘要；带 symbol_name 时带 Call chain。 |
| **apply_patch** | 唯一写入口：unified diff 创建/修改/删除；建议复杂补丁先 `dry_run: true` 校验再应用。 |
| **run_command** | 在项目根执行 shell 命令（构建、测试、安装等），不负责改文件。 |
| **list_project_files** | 列出目录树，支持 path、max_depth，用于探索结构。 |
| **grep** | 在项目里按文本/正则搜索，返回 文件:行:内容；不知道在哪个文件时先 grep 再 read_code_block。 |
| **attempt** | 维护当前用户意图与任务状态（存于 .photon/current_attempt.json）。每轮 get 回忆任务，按 intent/read_scope 推进；update 记录进度或完成；clear 清空以便新需求。 |
| **syntax_check** | 支持 C++、C、Python、ArkTS、TypeScript；**仅检测修改过的文件**（git，可关）。返回前 N 行或仅错误行，省 token。 |

（另有 skill_activate / skill_deactivate，按需注册后可加载技能。）

### 1. **read_code_block** - 智能代码读取
- **功能**: 智能读取代码文件，根据需求自动选择最佳策略
- **智能特性**:
  - 无参数 + 代码文件 → 返回符号摘要
  - 指定 `symbol_name` → 返回符号代码
  - 指定 `start_line/end_line` → 返回指定行范围
  - 其他情况 → 返回全文
- **用途**: 代码审查、符号查找、行级编辑

### 2. **apply_patch** - 统一补丁应用（仅 unified diff）
- **功能**: **只接受 Git unified diff**，一次可更新多个文件、多处修改
- **输入**: `diff_content`（必填），可选 `backup`、`dry_run`
- **多文件 diff 书写规范**（从源头避免 corrupt/trailing whitespace）：(1) hunk 内每行必须以且仅以 ` `、`+`、`-` 之一开头，**不要出现空行**；(2) 每行**不要有行尾空格**；(3) 多个文件时，上一个文件的最后一行 hunk 后**直接**接下一个文件的 `diff --git` 或 `--- /dev/null`，**中间不要空行**。复杂或多文件补丁建议先 `dry_run: true` 再应用。
- **Git优先**: 有 Git 时走 `git apply` / `git apply --check`，并把最近补丁保存到 `.photon/patches/last.patch` 供 `undo` 反向应用
- **无Git回退**: 没有 Git 时使用内置 unified-diff 引擎按 hunk 应用（要求上下文严格匹配）

### 3. **run_command** - 命令执行
- **功能**: 执行系统命令
- **特性**: 极简设计，只执行命令不做过滤
- **安全检查**: 由Agent层完成安全验证
- **用途**: 构建、测试、系统操作

### 4. **list_project_files** - 项目文件浏览
- **功能**: 列出项目目录结构
- **特性**: 支持递归目录遍历和深度控制
- **用途**: 项目结构探索、文件发现

### 5. **skill_activate** - 技能激活
- **功能**: 动态激活指定的Skill
- **用途**: 按需加载专业技能

### 6. **skill_deactivate** - 技能停用
- **功能**: 停用指定的Skill，释放上下文
- **用途**: 资源管理、上下文清理

### 7. **syntax_check** - 少 token 语法/构建检查（多语言 + 仅改动的文件）
- **支持语言**: C++、C、Python、ArkTS（.ets）、TypeScript（.ts/.tsx）。按扩展名识别。
- **仅检测修改文件**: 默认 `modified_only: true`，用 `git diff --name-only` 取修改/新增文件，只对这些文件做检查并只保留与之相关的错误行，进一步省 token。C/C++ 仍跑一次 `cmake --build`，输出过滤为仅含修改文件路径的行；Python 对每个修改的 .py 跑 `python3 -m py_compile`；TS 跑 `npx tsc --noEmit` 后过滤；ArkTS 若有 `ets2panda` 则对每个 .ets 检查。
- **参数**: `modified_only`（默认 true）、`max_output_lines`（默认 60）、`errors_only`（只保留错误行）、`build_dir`（C/C++ 构建目录）
- **用途**: 改代码后只检查改动文件语法，用较少 token 发现编译/语法问题

### 8. （已移除）
- `git_diff` 工具已移除，能力统一收敛到 `apply_patch`

## 🔌 MCP工具 (外部工具)

通过MCP (Model Context Protocol) 接入的外部工具，在配置中定义。

### 当前配置中的工具
根据 `tests/test_config.json` 配置，支持的工具包括：

#### 内置工具 (当 `use_builtin_tools: true`)
- **搜索工具**: Web搜索、代码搜索
- **内存工具**: 长期记忆存储和检索
- **任务调度**: 定时任务管理

#### 外部MCP服务器工具
- **文件系统工具**: 高级文件操作
- **数据库工具**: 数据库查询和管理
- **API工具**: 外部API调用
- **专业工具**: 领域特定工具

## 📖 预演：Photon 如何「阅读」一个项目

下面是一次典型的「摸清项目」时工具使用顺序（只读、不写）：

| 步骤 | 工具 | 典型调用 | 目的与下一步 |
|------|------|----------|--------------|
| **1. 看结构** | `list_project_files` | `path: "."`, `max_depth: 3` | 拿到目录树，识别根里有哪些模块（如 `src/`、`tests/`、`CMakeLists.txt`）。→ 决定先看哪几个目录/文件。 |
| **2. 看入口/构建** | `read_code_block` | `file_path: "CMakeLists.txt"` 或 `main.cpp`（无 symbol 参数） | 若是代码文件且无参数 → 得到**符号摘要**（有哪些函数/类）。→ 锁定入口（如 `main`）和关键模块名。 |
| **3. 看入口实现** | `read_code_block` | `file_path: "src/core/main.cpp"`, `symbol_name: "main"` | 得到 `main` 的代码，并附带 **Call chain**：Calls / Called by。→ 知道 main 调了谁、谁调了 main，决定沿哪条调用链往下追。 |
| **4. 沿调用链读** | `read_code_block` | `file_path: "src/tools/CoreTools.cpp"`, `symbol_name: "execute"` | 对 Call chain 里出现的符号逐个用 `symbol_name` 读，每次都会带 **Calls / Called by**。→ 递归理解「从 main 到叶子」的调用关系。 |
| **5. 看某段逻辑** | `read_code_block` | `file_path: "…", start_line: 100, end_line: 150` | 当已知行号或只需看一段时，用行范围精读。→ 不拉全文，省 token。 |
| **6. 确认能构建** | `run_command` | `command: "cmake -B build && cmake --build build"` 或 `npm install && npm run build` | 验证当前项目能编过、依赖是否齐全。→ 阅读阶段可不写代码，只读 + 构建验证。 |

**小结**：先 `list_project_files` 看骨架 → 用 `read_code_block` 无参数拿符号摘要 → 对入口/关键符号带 `symbol_name` 读代码并利用 **Call chain** 沿调用链往下读 → 需要时用 `start_line/end_line` 精读 → 最后用 `run_command` 确认能构建。全程只读时**不调用** `apply_patch`。

---

## 📖 预演：先读懂需求，再修改项目

用户提出需求（例如「给 apply_patch 失败时增加重试」）后，工具使用的典型顺序：

| 阶段 | 工具 | 典型调用 | 目的 |
|------|------|----------|------|
| **1. 拆解需求** | （无工具） | 从用户输入提炼：改什么行为、动哪些模块、有无边界情况 | 得到「要改的语义」和候选文件/符号，避免盲目全读。 |
| **2. 定位相关代码** | `list_project_files` | `path: "src/tools"`, `max_depth: 2` | 缩小到可能相关的目录（如工具、核心入口）。 |
| **3. 读相关符号摘要** | `read_code_block` | `file_path: "src/tools/CoreTools.cpp"`（无 symbol） | 拿到该文件的函数/类列表，找到和需求相关的符号（如 `ApplyPatchTool::execute`）。 |
| **4. 读实现 + 调用链** | `read_code_block` | `file_path: "src/tools/CoreTools.cpp"`, `symbol_name: "execute"` | 看具体逻辑和 **Call chain**，确定「在哪里加逻辑、依赖谁」；需要时对 Calls 里的符号再读。 |
| **5. 精读要改的段落** | `read_code_block` | `file_path: "…", start_line: 1230, end_line: 1260` | 对准要改的那几行，减少上下文噪音，便于生成精确 diff。 |
| **6. 写出并应用修改** | `apply_patch` | `diff_content: "--- a/src/tools/CoreTools.cpp\n+++ b/…\n@@ -1234,6 +1234,12 @@\n ..."` | 用**一条** unified diff 描述改动（可多文件、多 hunk）；一次 apply，备份/undo 由工具保证。 |
| **7. 校验改动** | `read_code_block` | 对刚改的文件再读对应 `symbol_name` 或行范围 | 确认代码和 Call chain 符合预期，没有漏改。 |
| **8. 构建与测试** | `run_command` 或 **syntax_check** | 快速检查语法：`syntax_check`（可选 `errors_only: true`、`max_output_lines: 40`）省 token；完整构建/测试用 `run_command`（如 `./build.sh`、`./build/agent_tests --gtest_filter=*ApplyPatch*`）。 | 确保能编过、相关测试通过。 |
| **9. 若失败** | `read_code_block` + `apply_patch` | 根据 run_command 报错或测试失败，再读报错位置、再生成一版 diff 修复 | 小步迭代，每次 diff 都可逆。 |

**小结**：**读懂需求**（拆解 + 定位）→ **读项目**（list → read 摘要 → read 符号+Call chain → 精读行范围）→ **改项目**（apply_patch，优先单次多文件）→ **验证**（read 检查 + run_command 构建/测试）→ 有问题再读、再改。这样「先读懂再改」形成闭环。

### 如何拆解需求（少走弯路、少读无关代码）

在动手读代码前，先把「用户说了什么」变成「要动哪些地方」：

1. **行为变化**：用户想要的结果是什么、和现在比「多/少/改」了哪一步行为？（例如：「失败时重试」= 多一步「检测失败 → 再试」。）
2. **入口与模块**：这个行为最可能从哪个入口触发？（例如：apply_patch 失败 → 入口是 `ApplyPatchTool::execute`。）结合项目结构猜 1～2 个目录（如 `src/tools`）。
3. **候选文件与符号**：在上述目录下，哪些文件、哪些函数/类名和该行为直接相关？列成清单（如 `CoreTools.cpp` 里的 `execute`），**只读清单里的**，不要先全库扫。
4. **边界与验收**：有无「必须不做的」或「必须额外处理的」？（例如：重试次数上限、只对某种错误重试。）后面写 diff 和测试时对着这些验收。

拆完后再按预演顺序：list 只扫相关目录 → read 只读清单里的文件/符号 → 精读只读要改的那几段。这样**先拆解再读**，避免「先读一大片再想改哪」。

### 如何减少重复读、省 Token

同一轮对话里尽量「读一次、用多次」：

- **先规划读范围**：按上面拆解出的「候选文件/符号」列一个**本次要读的列表**，按列表顺序读，读到的内容在上下文中复用；不要同一文件先无参数读摘要、后面又全文读。
- **用符号/行范围代替全文**：能确定符号名就用 `symbol_name` 只读该符号；能确定行号就用 `start_line/end_line` 只读那一段；避免对同一文件多次「无参数全文」。
- **Call chain 一次读出**：带 `symbol_name` 时已经带回 Calls/Called by，沿调用链往下读时**只对新出现的符号**再调 read，不要对已出现过的符号重复读。
- **校验时只读改动的片段**：apply_patch 之后校验时，只对**改动的文件 + 改动的符号或行范围**再读一次，不要重新读整个文件。
- **一次 diff 多文件**：能在一个需求里改多处的，尽量**一条 apply_patch 里多段 diff**，减少「改 A → 读 B → 再改 B」的多轮读-改，也减少总轮数。

若需求复杂、多轮对话，可在**首轮**用 list + 少量 read 做「需求拆解 + 读范围规划」，把「要读的文件/符号清单」和「要改的语义」总结进回复，后续轮次严格按清单读、按语义改，避免反复重读同一块项目。

---

## 🤝 核心工具协作：新建/搭建项目

新建或从零搭建项目时，四个核心工具典型配合方式如下：

| 阶段 | 工具 | 作用 |
|------|------|------|
| **摸底** | `list_project_files` | 看当前目录结构（path、max_depth），判断是空目录还是已有骨架 |
| **创建/修改** | `apply_patch` | 用**一份** unified diff 创建或修改多个文件（多个 `---`/`+++` 段）；新建文件用 `--- /dev/null` + `@@ -0,0 +1,N @@` |
| **校验** | `read_code_block` | 读刚改过的文件或符号，确认内容是否符合预期、是否需要再改 |
| **构建/测试** | `run_command` | 在项目根目录执行 `npm init`、`cmake`、`cargo build`、`pytest` 等，看能否通过 |

**推荐顺序**：先 `list_project_files` 看现状 → 用一次或多次 `apply_patch` 搭好骨架/代码 → 用 `read_code_block` 抽检关键文件 → 用 `run_command` 构建并测试。所有写操作只通过 `apply_patch`，便于回滚和与 Git 一致。

---

## ⚠️ 相对「读懂需求→改项目→省 Token」目标，Photon 当前不足

按「需求拆解 → 按范围读 → 改 → 验证」这条链路，目前还存在以下缺口或可改进点：

| 方面 | 不足 | 可能方向 |
|------|------|----------|
| **需求拆解与计划** | Constitution 要求「先规划、标 entry symbols / affected files」，但**没有结构化计划输出或持久化**；Agent 是否先规划全靠自觉，多轮后容易忘。 | 增加「计划」产出：例如要求首轮输出 `plan: { entry_symbols, affected_files, read_scope_list }` 并写入 ProjectMemory 或会话状态；或提供 `set_task_plan` / `get_task_plan` 类工具，把「本次要读的清单」固化下来。 |
| **跨轮记忆** | 上下文超阈值会压缩（ContextManager），**已读过的文件/符号没有单独沉淀**；新轮可能重复读同一块。 | 用 ProjectMemory 或新结构存「本任务已读文件/符号」摘要或清单，新轮先查再决定是否读；或提供「当前任务摘要」占位，压缩时优先保留。 |
| **代码搜索** | **semantic_search 未注册**，默认只有 list + read；「不知道在哪个文件」时只能靠目录+多次 read，费 token。 | 在 main 中注册 SemanticSearchTool（或通过 MCP 提供代码搜索），便于「先搜到文件/符号再精读」。 |
| **Diff 质量** | LLM 生成的 diff 常出现格式问题（corrupt patch、already exists、上下文不匹配），导致多次重试和额外读。 | 在 Constitution/工具描述中强化 diff 格式与示例；或提供 `dry_run: true` 默认/推荐，先校验再真正 apply；把 apply 报错信息更结构化地回传给模型（已部分做了）。 |
| **索引与 Call chain** | 调用链依赖 SymbolManager 索引；**索引可能未扫完或过期**，Call chain 为空时用户无感知。 | 在 read_code_block 返回中标注「Call chain 来自索引，若为空可能未扫描/过期」；或提供 `index_status` 类查询，提示是否需重新扫描。 |
| **技能与高层能力** | skill_activate / skill_deactivate 未在主流程注册，内置技能（如架构重组、配置编辑）**默认不可用**。 | 若希望「拆解需求」「按模板改」等由技能承载，需在 main 中注册技能工具并保证技能里也遵守读范围与 apply_patch 约定。 |
| **验证自动化** | 改完后「跑哪些测试、看哪段日志」靠模型自己选，没有「按改动文件推荐测试」的机制。 | 可选：根据 apply_patch 的 affected_files 推荐 run_command（如对应 test 目标），或提供 `suggest_verify_commands` 类提示。 |

**小结**：与目标最相关的几块是——**把「计划/读范围」结构化并持久化**、**跨轮不丢「已读清单」**、**默认可用代码搜索**、**提升 diff 一次通过率**。其余为增强项。补上这些后，「先读懂需求再改项目、少浪费 token」的闭环会更稳。

---

## 🎯 工具架构特点

### 极简设计原则
1. **单一职责**: 每个工具只做一件事，做到极致简单
2. **原子操作**: 工具之间可以组合使用，避免复杂逻辑
3. **智能组合**: Agent层负责智能组合工具完成复杂任务

### 安全机制
1. **Constitution校验**: 所有工具调用都通过宪法验证
2. **人工确认**: 高风险操作需要用户确认
3. **权限控制**: 细粒度的工具权限管理

### 跨平台支持
- ✅ Windows (包括Windows Git修复)
- ✅ macOS
- ✅ Linux
- 自动检测平台并适配命令格式

## 📊 工具统计

- **核心工具**: 6个（`apply_patch` 统一补丁；不再提供 `git_diff`）
- **MCP工具**: 根据配置动态加载
- **总工具数**: 核心工具 + 动态MCP工具

## 🔧 工具使用示例

### 在CLI中查看工具
```
❯ tools
```

### 工具调用示例
```json
{
  "type": "function",
  "function": {
    "name": "read_code_block",
    "arguments": {
      "path": "src/main.cpp",
      "mode": {
        "type": "symbol",
        "name": "main"
      }
    }
  }
}
```

## 🚀 扩展工具

新的工具可以通过以下方式添加：
1. **核心工具**: 继承 `ITool` 接口，注册到 `ToolRegistry`
2. **MCP工具**: 配置MCP服务器连接
3. **技能工具**: 通过Skill系统动态加载

工具系统采用插件化架构，支持运行时动态加载和卸载。