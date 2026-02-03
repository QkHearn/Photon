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
     */
    AgentRuntime(
        std::shared_ptr<LLMClient> llmClient,
        ToolRegistry& toolRegistry,
        SymbolManager* symbolManager = nullptr,
        MemoryManager* memoryManager = nullptr,
        SkillManager* skillManager = nullptr
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
    
    AgentState state;
    nlohmann::json messageHistory;
    
    int maxIterations = 50;
    
    // 工具 Schema 缓存
    std::vector<nlohmann::json> toolSchemas;
};
