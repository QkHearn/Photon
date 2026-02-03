#include "AgentRuntime.h"
#include "utils/SkillManager.h"
#include "analysis/SymbolManager.h"
#include "analysis/SemanticManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <cctype>

// 临时: 记忆管理器前向声明
class MemoryManager {
public:
    // TODO: 实现接口
};

AgentRuntime::AgentRuntime(
    std::shared_ptr<LLMClient> llmClient,
    ToolRegistry& toolRegistry,
    SymbolManager* symbolManager,
    MemoryManager* memoryManager,
    SkillManager* skillManager,
    SemanticManager* semanticManager
) : llm(llmClient),
    tools(toolRegistry),
    symbolMgr(symbolManager),
    memory(memoryManager),
    skillMgr(skillManager),
    semanticMgr(semanticManager) {
    
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
            for (auto& toolCall : message["tool_calls"]) {
                // 🆕 拦截 read_file 请求,主动进行 AST 分析
                if (symbolMgr) {
                    interceptAndAnalyzeFileRead(toolCall);
                }
                // 🆕 拦截查询请求,主动进行语义搜索
                if (semanticMgr) {
                    interceptAndEnhanceQuery(toolCall);
                }
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
    if (!symbolMgr) return {};
    
    auto symbols = symbolMgr->search(query);
    std::vector<nlohmann::json> results;
    
    for (const auto& sym : symbols) {
        nlohmann::json item;
        item["name"] = sym.name;
        item["type"] = sym.type;
        item["path"] = sym.path;
        item["line"] = sym.line;
        item["endLine"] = sym.endLine;
        item["signature"] = sym.signature;
        results.push_back(item);
    }
    
    return results;
}

std::string AgentRuntime::findSymbolLocation(const std::string& symbolName) {
    if (!symbolMgr) return "";
    
    auto symbols = symbolMgr->search(symbolName);
    if (symbols.empty()) return "";
    
    const auto& sym = symbols[0];
    return sym.path + ":" + std::to_string(sym.line);
}

// ========== AST 分析能力实现 ==========

void AgentRuntime::interceptAndAnalyzeFileRead(nlohmann::json& toolCall) {
    if (!toolCall.contains("function")) return;
    
    auto& func = toolCall["function"];
    std::string toolName = func.contains("name") ? func["name"].get<std::string>() : "";
    
    // 只拦截 read_file 相关工具
    if (toolName != "read_file" && toolName != "read_code_file") return;
    
    // 提取文件路径参数
    std::string argsStr = func.contains("arguments") ? func["arguments"].get<std::string>() : "{}";
    nlohmann::json args;
    try {
        args = nlohmann::json::parse(argsStr);
    } catch (...) {
        return;
    }
    
    std::string filePath;
    if (args.contains("path")) {
        filePath = args["path"].get<std::string>();
    } else if (args.contains("file_path")) {
        filePath = args["file_path"].get<std::string>();
    } else if (args.contains("file")) {
        filePath = args["file"].get<std::string>();
    }
    
    if (filePath.empty()) return;
    
    std::cout << "[Agent] 🔍 Intercepted file read: " << filePath << std::endl;
    std::cout << "[Agent] 🧠 Performing AST analysis..." << std::endl;
    
    // 生成符号摘要
    std::string symbolSummary = generateSymbolSummary(filePath);
    
    if (!symbolSummary.empty()) {
        // 将符号信息注入为系统提示
        nlohmann::json hintMsg;
        hintMsg["role"] = "system";
        hintMsg["content"] = "📊 [Agent Analysis] File structure for `" + filePath + "`:\n\n" + symbolSummary + 
                            "\n\n💡 You can now ask to see specific symbols instead of reading the entire file.";
        messageHistory.push_back(hintMsg);
        
        std::cout << "[Agent] ✅ Symbol summary injected (" << symbolSummary.length() << " chars)" << std::endl;
    }
}

std::string AgentRuntime::generateSymbolSummary(const std::string& filePath) {
    if (!symbolMgr) return "";
    
    auto symbols = symbolMgr->getFileSymbols(filePath);
    if (symbols.empty()) return "";
    
    std::ostringstream summary;
    
    // 按类型分组
    std::map<std::string, std::vector<const Symbol*>> grouped;
    for (const auto& sym : symbols) {
        grouped[sym.type].push_back(&sym);
    }
    
    // 格式化输出
    int totalSymbols = 0;
    for (const auto& [type, syms] : grouped) {
        if (syms.empty()) continue;
        
        summary << "### " << type << "s (" << syms.size() << "):\n";
        
        for (const auto* sym : syms) {
            summary << "  - `" << sym->name << "`";
            if (!sym->signature.empty() && sym->signature != sym->name) {
                summary << " - " << sym->signature;
            }
            summary << " (lines " << sym->line << "-" << sym->endLine << ")";
            summary << " [" << sym->source << "]\n";
            totalSymbols++;
            
            // 限制每个类型最多显示 20 个
            if (totalSymbols >= 20) break;
        }
        
        if (totalSymbols >= 20) {
            summary << "  ... (truncated, " << (symbols.size() - totalSymbols) << " more symbols)\n";
            break;
        }
    }
    
    return summary.str();
}

nlohmann::json AgentRuntime::getSymbolCodeBlock(const std::string& filePath, const std::string& symbolName) {
    nlohmann::json result;
    
    if (!symbolMgr) {
        result["error"] = "SymbolManager not available";
        return result;
    }
    
    // 查找符号
    auto symbols = symbolMgr->getFileSymbols(filePath);
    const Symbol* targetSymbol = nullptr;
    
    for (const auto& sym : symbols) {
        if (sym.name == symbolName) {
            targetSymbol = &sym;
            break;
        }
    }
    
    if (!targetSymbol) {
        result["error"] = "Symbol '" + symbolName + "' not found in " + filePath;
        return result;
    }
    
    // 返回符号信息
    result["symbol_name"] = targetSymbol->name;
    result["type"] = targetSymbol->type;
    result["file_path"] = filePath;
    result["start_line"] = targetSymbol->line;
    result["end_line"] = targetSymbol->endLine;
    result["signature"] = targetSymbol->signature;
    result["source"] = targetSymbol->source;
    
    // 提示: 可以用这些行号读取具体代码
    result["hint"] = "Use read_file with start_line=" + std::to_string(targetSymbol->line) + 
                     " and end_line=" + std::to_string(targetSymbol->endLine) + " to read this symbol's code";
    
    return result;
}

bool AgentRuntime::hasSimilarFailure(const std::string& error) {
    // TODO: 实现失败记忆查询
    return false;
}

std::string AgentRuntime::getFailureSolution(const std::string& error) {
    // TODO: 实现解决方案查询
    return "";
}

// ========== 语义搜索能力实现 ==========

void AgentRuntime::interceptAndEnhanceQuery(nlohmann::json& toolCall) {
    if (!semanticMgr) return;
    if (!toolCall.contains("function")) return;
    
    auto& func = toolCall["function"];
    std::string toolName = func.contains("name") ? func["name"].get<std::string>() : "";
    
    // 场景 1: 拦截 read_file 工具,检测是否是模糊查询
    if (toolName == "read_code_block" || toolName == "read_file") {
        std::string argsStr = func.contains("arguments") ? func["arguments"].get<std::string>() : "{}";
        nlohmann::json args;
        try {
            args = nlohmann::json::parse(argsStr);
        } catch (...) {
            return;
        }
        
        std::string path;
        if (args.contains("path")) {
            path = args["path"].get<std::string>();
        } else if (args.contains("file_path")) {
            path = args["file_path"].get<std::string>();
        }
        
        if (path.empty()) return;
        
        // 启发式检测: 路径包含空格、中文、疑问词等 → 可能是语义查询
        bool hasSpace = path.find(' ') != std::string::npos;
        bool hasChinese = std::any_of(path.begin(), path.end(), [](unsigned char c) { return c > 127; });
        bool hasQuestion = path.find('?') != std::string::npos || 
                          path.find("where") != std::string::npos ||
                          path.find("what") != std::string::npos ||
                          path.find("how") != std::string::npos ||
                          path.find("哪") != std::string::npos ||
                          path.find("什么") != std::string::npos ||
                          path.find("如何") != std::string::npos;
        
        if (hasSpace || hasChinese || hasQuestion) {
            std::cout << "[Agent] 🔍 Detected semantic query in path: " << path << std::endl;
            
            // 执行语义搜索
            std::string searchResults = performSemanticSearch(path);
            
            if (!searchResults.empty()) {
                nlohmann::json hintMsg;
                hintMsg["role"] = "system";
                hintMsg["content"] = "🔎 [Agent Semantic Search] " + searchResults;
                messageHistory.push_back(hintMsg);
                
                std::cout << "[Agent] ✅ Semantic search results injected" << std::endl;
            }
        }
    }
    
    // 场景 2: 检测 list_project_files 是否带有查询意图
    if (toolName == "list_project_files") {
        std::string argsStr = func.contains("arguments") ? func["arguments"].get<std::string>() : "{}";
        nlohmann::json args;
        try {
            args = nlohmann::json::parse(argsStr);
        } catch (...) {
            return;
        }
        
        // 如果有 query 或 pattern 参数,也可以触发语义搜索
        if (args.contains("query") || args.contains("pattern")) {
            std::string query = args.value("query", args.value("pattern", ""));
            if (!query.empty()) {
                std::cout << "[Agent] 🔍 Detected semantic query in list request: " << query << std::endl;
                
                std::string searchResults = performSemanticSearch(query);
                
                if (!searchResults.empty()) {
                    nlohmann::json hintMsg;
                    hintMsg["role"] = "system";
                    hintMsg["content"] = "🔎 [Agent Semantic Search] " + searchResults;
                    messageHistory.push_back(hintMsg);
                    
                    std::cout << "[Agent] ✅ Semantic search results injected" << std::endl;
                }
            }
        }
    }
}

std::string AgentRuntime::performSemanticSearch(const std::string& query, int topK) {
    if (!semanticMgr) return "";
    
    std::cout << "[Agent] 🧠 Performing semantic search for: \"" << query << "\"" << std::endl;
    
    try {
        // 调用语义搜索
        auto chunks = semanticMgr->search(query, topK);
        
        if (chunks.empty()) {
            std::cout << "[Agent] ⚠️  No semantic results found" << std::endl;
            return "";
        }
        
        std::cout << "[Agent] 📊 Found " << chunks.size() << " relevant chunks" << std::endl;
        
        // 格式化结果
        std::ostringstream result;
        result << "Found " << chunks.size() << " relevant code locations for query: \"" << query << "\"\n\n";
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            
            result << "**[" << (i + 1) << "] " << chunk.path 
                   << " (lines " << chunk.startLine << "-" << chunk.endLine << ")**\n";
            result << "   Relevance: " << std::fixed << std::setprecision(2) << (chunk.score * 100) << "%\n";
            result << "   Type: " << chunk.type << "\n";
            
            // 显示代码片段预览（前 4 行或 200 字符）
            std::istringstream contentStream(chunk.content);
            std::string line;
            int lineCount = 0;
            int charCount = 0;
            result << "   Preview:\n";
            
            while (std::getline(contentStream, line) && lineCount < 4 && charCount < 200) {
                // 去除前后空白
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    line = line.substr(start);
                }
                
                if (!line.empty()) {
                    result << "     " << line << "\n";
                    charCount += line.length();
                    lineCount++;
                }
            }
            
            if (lineCount >= 4 || charCount >= 200) {
                result << "     ...\n";
            }
            result << "\n";
        }
        
        result << "💡 **Tip**: Use `read_code_block` with the file path and line numbers above to see the full code.\n";
        
        return result.str();
        
    } catch (const std::exception& e) {
        std::cerr << "[Agent] ❌ Semantic search error: " << e.what() << std::endl;
        return "";
    }
}

std::string AgentRuntime::detectSemanticQueryIntent(const std::string& content) {
    // 检测用户消息中的查询意图关键词
    std::vector<std::string> queryKeywords = {
        "where is", "find", "search", "locate", "show me",
        "在哪", "找到", "查找", "定位", "显示",
        "how to", "如何", "怎么",
        "what is", "什么是", "是什么"
    };
    
    std::string lowerContent = content;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    
    for (const auto& keyword : queryKeywords) {
        size_t pos = lowerContent.find(keyword);
        if (pos != std::string::npos) {
            // 提取查询的主题（简单实现：取关键词后的一段文本）
            size_t startPos = pos + keyword.length();
            size_t endPos = lowerContent.find_first_of(".,?!\n", startPos);
            if (endPos == std::string::npos) endPos = lowerContent.length();
            
            std::string query = content.substr(startPos, endPos - startPos);
            // 去除前后空白
            query.erase(0, query.find_first_not_of(" \t\n"));
            query.erase(query.find_last_not_of(" \t\n") + 1);
            
            if (!query.empty()) {
                return query;
            }
        }
    }
    
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
