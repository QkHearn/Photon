#include "AgentRuntime.h"
#include "utils/SkillManager.h"
#include <iostream>
#include <sstream>

// 临时: 符号管理器前向声明
class SymbolManager {
public:
    // TODO: 实现接口
};

class MemoryManager {
public:
    // TODO: 实现接口
};

AgentRuntime::AgentRuntime(
    std::shared_ptr<LLMClient> llmClient,
    ToolRegistry& toolRegistry,
    SymbolManager* symbolManager,
    MemoryManager* memoryManager,
    SkillManager* skillManager
) : llm(llmClient),
    tools(toolRegistry),
    symbolMgr(symbolManager),
    memory(memoryManager),
    skillMgr(skillManager) {
    
    // 缓存工具 Schema
    toolSchemas = tools.listToolSchemas();
    
    // 初始化消息历史
    messageHistory = nlohmann::json::array();
}

void AgentRuntime::executeTask(const std::string& userGoal) {
    // 重置状态
    state.reset();
    state.taskGoal = userGoal;
    
    // 添加系统 Prompt
    nlohmann::json systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = assembleSystemPrompt();
    messageHistory.push_back(systemMsg);
    
    // 添加用户任务
    nlohmann::json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userGoal;
    messageHistory.push_back(userMsg);
    
    // 启动主循环
    runLoop();
}

void AgentRuntime::runLoop() {
    while (!state.isComplete && state.iteration < maxIterations) {
        state.iteration++;
        
        std::cout << "\n[Agent] Iteration " << state.iteration << "/" << maxIterations << std::endl;
        
        // Phase 1: Planning
        state.currentPhase = "planning";
        planPhase();
        
        // Phase 2: Acting
        state.currentPhase = "acting";
        actPhase();
        
        // Phase 3: Observing
        state.currentPhase = "observing";
        observePhase();
        
        // 检查是否完成
        // TODO: 更智能的完成判断
        if (state.plannedActions.empty()) {
            state.isComplete = true;
        }
    }
    
    if (state.iteration >= maxIterations) {
        std::cout << "\n[Agent] Maximum iterations reached." << std::endl;
    } else {
        std::cout << "\n[Agent] Task completed." << std::endl;
    }
}

void AgentRuntime::planPhase() {
    std::cout << "[Agent] Planning..." << std::endl;
    
    // TODO: 检查 LLM 是否需要符号信息
    // 如果需要,Agent 内部查询并添加提示
    
    // 动态注入激活的 Skill Prompt
    // 动态注入激活的 Skill Prompt
    if (skillMgr) {
        std::string activeSkillsPrompt = skillMgr->getActiveSkillsPrompt();
        if (!activeSkillsPrompt.empty()) {
            nlohmann::json skillPromptMsg;
            skillPromptMsg["role"] = "system";
            skillPromptMsg["content"] = activeSkillsPrompt;
            messageHistory.push_back(skillPromptMsg);
        }
    }
    
    // 调用 LLM
    try {
        nlohmann::json llmTools = nlohmann::json::array();
        for (const auto& schema : toolSchemas) {
            llmTools.push_back(schema);
        }
        
        nlohmann::json response = llm->chatWithTools(messageHistory, llmTools);
        
        if (response.is_null() || !response.contains("choices") || response["choices"].empty()) {
            std::cout << "[Agent] No response from LLM" << std::endl;
            state.isComplete = true;
            return;
        }
        
        auto& choice = response["choices"][0];
        auto& message = choice["message"];
        
        // 保存 LLM 响应
        messageHistory.push_back(message);
        
        // 提取工具调用
        state.plannedActions.clear();
        if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
            for (const auto& toolCall : message["tool_calls"]) {
                state.plannedActions.push_back(toolCall);
            }
            std::cout << "[Agent] Planned " << state.plannedActions.size() << " actions" << std::endl;
        } else {
            // 没有工具调用,可能是最终回答
            if (message.contains("content") && !message["content"].is_null()) {
                std::string content = message["content"].is_string() 
                    ? message["content"].get<std::string>() 
                    : message["content"].dump();
                std::cout << "[Agent] LLM Response: " << content << std::endl;
            }
            state.isComplete = true;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Agent] Planning failed: " << e.what() << std::endl;
        state.isComplete = true;
    }
}

void AgentRuntime::actPhase() {
    if (state.plannedActions.empty()) {
        std::cout << "[Agent] No actions to execute" << std::endl;
        return;
    }
    
    std::cout << "[Agent] Executing " << state.plannedActions.size() << " actions..." << std::endl;
    
    state.observations.clear();
    
    for (const auto& toolCall : state.plannedActions) {
        std::string toolName = toolCall["function"]["name"].get<std::string>();
        std::string argsStr = toolCall["function"]["arguments"].get<std::string>();
        
        std::cout << "[Agent]   - " << toolName << std::endl;
        
        nlohmann::json args;
        try {
            args = nlohmann::json::parse(argsStr);
        } catch (...) {
            args = nlohmann::json::object();
            std::cerr << "[Agent]   ! Failed to parse arguments" << std::endl;
        }
        
        // 执行工具
        try {
            nlohmann::json result = tools.executeTool(toolName, args);
            
            // 检查是否失败
            if (result.contains("error")) {
                std::string error = result["error"].get<std::string>();
                std::cerr << "[Agent]   ! Tool failed: " << error << std::endl;
                
                // 记录失败
                state.recordFailure(toolName, args, error);
                
                // 检查是否有类似历史失败
                if (hasSimilarFailure(error)) {
                    std::string solution = getFailureSolution(error);
                    std::cout << "[Agent]   * Similar failure found. Solution: " << solution << std::endl;
                    result["failure_hint"] = solution;
                }
            }
            
            // 保存结果
            state.observations.push_back(result);
            
            // 添加到消息历史
            nlohmann::json toolResult;
            toolResult["role"] = "tool";
            toolResult["tool_call_id"] = toolCall["id"];
            toolResult["name"] = toolName;
            toolResult["content"] = result.dump();
            messageHistory.push_back(toolResult);
            
        } catch (const std::exception& e) {
            std::cerr << "[Agent]   ! Exception: " << e.what() << std::endl;
            
            nlohmann::json errorResult;
            errorResult["error"] = std::string("Exception: ") + e.what();
            state.observations.push_back(errorResult);
            
            // 添加错误到消息历史
            nlohmann::json toolResult;
            toolResult["role"] = "tool";
            toolResult["tool_call_id"] = toolCall["id"];
            toolResult["name"] = toolName;
            toolResult["content"] = errorResult.dump();
            messageHistory.push_back(toolResult);
        }
    }
}

void AgentRuntime::observePhase() {
    std::cout << "[Agent] Observing results..." << std::endl;
    
    // 分析结果
    int successCount = 0;
    int failureCount = 0;
    
    for (const auto& obs : state.observations) {
        if (obs.contains("error")) {
            failureCount++;
        } else {
            successCount++;
        }
    }
    
    std::cout << "[Agent] Results: " << successCount << " succeeded, " 
              << failureCount << " failed" << std::endl;
    
    // TODO: 更智能的观察逻辑
    // - 如果所有工具都失败,尝试改变策略
    // - 如果部分成功,继续计划
}

// ========== 内部能力实现 ==========

std::vector<nlohmann::json> AgentRuntime::querySymbols(const std::string& query) {
    // TODO: 实现符号查询
    // 这是 Agent 的私有能力,LLM 看不到
    return {};
}

std::string AgentRuntime::findSymbolLocation(const std::string& symbolName) {
    // TODO: 实现符号位置查找
    // 这是 Agent 的私有能力,LLM 看不到
    return "";
}

bool AgentRuntime::hasSimilarFailure(const std::string& error) {
    // TODO: 实现失败记忆查询
    return false;
}

std::string AgentRuntime::getFailureSolution(const std::string& error) {
    // TODO: 实现解决方案查询
    return "";
}

// ========== Prompt 组装 ==========

std::string AgentRuntime::assembleSystemPrompt() {
    std::ostringstream prompt;
    
    prompt << "You are Photon, an autonomous AI agent specialized in software engineering tasks.\n\n";
    
    prompt << "Your capabilities:\n";
    prompt << "- You have access to " << toolSchemas.size() << " tools for code manipulation\n";
    prompt << "- You can read, write, execute commands, and navigate project structures\n";
    prompt << "- You must plan carefully before taking actions\n\n";
    
    prompt << "Core principles:\n";
    prompt << "1. THINK STEP-BY-STEP: Always explain your reasoning before acting\n";
    prompt << "2. USE TOOLS WISELY: Tools are atomic operations - combine them intelligently\n";
    prompt << "3. LEARN FROM FAILURES: If a tool fails, try a different approach\n";
    prompt << "4. BE PRECISE: Provide exact file paths and line numbers\n";
    prompt << "5. ASK WHEN UNCLEAR: If the task is ambiguous, ask for clarification\n\n";
    
    prompt << "Available tools:\n";
    for (size_t i = 0; i < toolSchemas.size(); ++i) {
        const auto& schema = toolSchemas[i];
        const auto& func = schema["function"];
        prompt << (i + 1) << ". " << func["name"].get<std::string>() 
               << ": " << func["description"].get<std::string>() << "\n";
    }
    
    // 注入 Skill 发现 Prompt (仅列表,不注入内容)
    if (skillMgr) {
        prompt << skillMgr->getSkillDiscoveryPrompt();
    }
    
    return prompt.str();
}

nlohmann::json AgentRuntime::assembleContext() {
    nlohmann::json context;
    
    context["task_goal"] = state.taskGoal;
    context["current_phase"] = state.currentPhase;
    context["iteration"] = state.iteration;
    context["completed_steps"] = state.completedSteps;
    context["failed_attempts_count"] = state.failedAttempts.size();
    
    return context;
}

// ========== Skill 管理实现 ==========

bool AgentRuntime::activateSkill(const std::string& skillName) {
    if (!skillMgr) {
        std::cerr << "[Agent] SkillManager not available" << std::endl;
        return false;
    }
    
    std::cout << "[Agent] Activating skill: " << skillName << std::endl;
    return skillMgr->activate(skillName);
}

void AgentRuntime::deactivateSkill(const std::string& skillName) {
    if (!skillMgr) return;
    
    std::cout << "[Agent] Deactivating skill: " << skillName << std::endl;
    skillMgr->deactivate(skillName);
}
