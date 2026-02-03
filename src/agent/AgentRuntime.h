#pragma once
#include <string>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include "AgentState.h"
#include "tools/ToolRegistry.h"
#include "core/LLMClient.h"

// 前向声明
class SymbolManager;
class MemoryManager;
class SkillManager;
class SemanticManager;

/**
 * @brief Agent 运行时 - 核心智能体主循环
 * 
 * 实现 Plan → Act → Observe 循环:
 * - Plan: 调用 LLM 生成执行计划
 * - Act: 执行工具调用
 * - Observe: 分析结果,决定下一步
 * 
 * 核心设计原则:
 * 1. LLM 只能看到工具,不能直接访问内部能力
 * 2. Symbol/LSP 等分析能力是 Agent 的私有能力
 * 3. 失败会被记录和学习
 */
class AgentRuntime {
public:
    /**
     * @brief 构造函数
     * @param llmClient LLM 客户端
     * @param toolRegistry 工具注册表
     * @param symbolManager 符号管理器 (可选)
     * @param memoryManager 记忆管理器 (可选)
     * @param skillManager Skill 管理器 (可选)
     * @param semanticManager 语义管理器 (可选)
     */
    AgentRuntime(
        std::shared_ptr<LLMClient> llmClient,
        ToolRegistry& toolRegistry,
        SymbolManager* symbolManager = nullptr,
        MemoryManager* memoryManager = nullptr,
        SkillManager* skillManager = nullptr,
        SemanticManager* semanticManager = nullptr
    );

    /**
     * @brief 执行用户任务
     * @param userGoal 用户目标描述
     * 
     * 这是主入口,会启动完整的 Plan-Act-Observe 循环
     */
    void executeTask(const std::string& userGoal);

    /**
     * @brief 获取当前状态
     */
    const AgentState& getState() const { return state; }

    /**
     * @brief 设置最大迭代次数
     */
    void setMaxIterations(int max) { maxIterations = max; }

    /**
     * @brief 获取消息历史 (用于调试)
     */
    const nlohmann::json& getMessageHistory() const { return messageHistory; }

private:
    // ========== 核心循环 ==========
    
    /**
     * @brief 主循环: Plan → Act → Observe
     */
    void runLoop();
    
    /**
     * @brief 计划阶段: 调用 LLM 生成计划
     * 
     * 在这个阶段:
     * 1. 组装 Prompt (包含任务目标、当前状态、工具列表)
     * 2. 如果 LLM 提到某个符号,Agent 内部查询并提示位置
     * 3. 调用 LLM 获取下一步计划
     */
    void planPhase();
    
    /**
     * @brief 执行阶段: 执行工具调用
     * 
     * 在这个阶段:
     * 1. 执行 LLM 规划的所有工具调用
     * 2. 记录每个工具的执行结果
     * 3. 如果失败,检查是否有类似历史失败
     */
    void actPhase();
    
    /**
     * @brief 观察阶段: 分析结果,决定下一步
     * 
     * 在这个阶段:
     * 1. 分析工具执行结果
     * 2. 更新任务状态
     * 3. 决定是继续还是结束
     */
    void observePhase();

    // ========== 内部能力 (不暴露给 LLM) ==========
    
    /**
     * @brief 查询符号
     * Agent 内部能力,LLM 看不到
     */
    std::vector<nlohmann::json> querySymbols(const std::string& query);
    
    /**
     * @brief 查找符号位置
     * Agent 内部能力,LLM 看不到
     */
    std::string findSymbolLocation(const std::string& symbolName);
    
    /**
     * @brief 从记忆中查找类似失败
     * Agent 内部能力,LLM 看不到
     */
    bool hasSimilarFailure(const std::string& error);
    
    /**
     * @brief 获取失败的解决方案
     * Agent 内部能力,LLM 看不到
     */
    std::string getFailureSolution(const std::string& error);
    
    // ========== AST 分析能力 (智能体主动分析) ==========
    
    /**
     * @brief 拦截并分析 read_file 请求
     * 在 LLM 读文件前,先提取符号信息并注入提示
     */
    void interceptAndAnalyzeFileRead(nlohmann::json& toolCall);
    
    /**
     * @brief 生成文件的符号摘要
     * 返回格式化的符号列表 (函数、类等)
     */
    std::string generateSymbolSummary(const std::string& filePath);
    
    /**
     * @brief 处理 LLM 选择的符号,返回对应代码块
     * @param filePath 文件路径
     * @param symbolName 符号名称
     * @return 包含行号范围的代码块信息
     */
    nlohmann::json getSymbolCodeBlock(const std::string& filePath, const std::string& symbolName);
    
    // ========== 语义搜索能力 (智能体主动搜索) ==========
    
    /**
     * @brief 拦截并增强模糊查询
     * 当检测到 LLM 的查询意图但不确定具体位置时,
     * Agent 主动使用语义搜索提供候选代码片段
     */
    void interceptAndEnhanceQuery(nlohmann::json& toolCall);
    
    /**
     * @brief 执行语义搜索 (内部能力)
     * @param query 自然语言查询
     * @param topK 返回前 K 个结果
     * @return 相关代码片段的格式化摘要
     */
    std::string performSemanticSearch(const std::string& query, int topK = 5);
    
    /**
     * @brief 检测用户消息是否包含语义查询意图
     * @param content 消息内容
     * @return 如果包含查询意图则返回提取的查询文本,否则返回空字符串
     */
    std::string detectSemanticQueryIntent(const std::string& content);

    // ========== Prompt 组装 ==========
    
    /**
     * @brief 组装系统 Prompt
     */
    std::string assembleSystemPrompt();
    
    /**
     * @brief 组装当前状态的上下文
     */
    nlohmann::json assembleContext();

    // ========== Skill 管理 ==========
    
    /**
     * @brief 激活 Skill
     * Agent 决定需要某个 Skill 时调用
     */
    bool activateSkill(const std::string& skillName);
    
    /**
     * @brief 停用 Skill
     */
    void deactivateSkill(const std::string& skillName);

    // ========== 成员变量 ==========
    
    std::shared_ptr<LLMClient> llm;
    ToolRegistry& tools;
    SymbolManager* symbolMgr;
    MemoryManager* memory;
    SkillManager* skillMgr;
    SemanticManager* semanticMgr;
    
    AgentState state;
    nlohmann::json messageHistory;
    
    int maxIterations = 50;
    
    // 工具 Schema 缓存
    std::vector<nlohmann::json> toolSchemas;
};
