#pragma once
#include <string>
#include <nlohmann/json.hpp>

/**
 * @brief 工具接口定义
 * 
 * 所有工具必须实现此接口。工具应该是"极简极笨"的原子操作,
 * 不应该包含任何智能判断逻辑。
 * 
 * 核心原则:
 * 1. 工具必须是原子的 (不可再分的基本操作)
 * 2. 工具必须是笨的 (不包含智能决策)
 * 3. 工具只执行,不判断 (判断由 Agent 完成)
 */
class ITool {
public:
    virtual ~ITool() = default;

    /**
     * @brief 获取工具名称
     * @return 工具的唯一标识名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取工具描述
     * @return 工具功能的简短描述 (用于 LLM 理解)
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief 获取工具的 JSON Schema
     * @return 符合 JSON Schema 规范的参数定义
     */
    virtual nlohmann::json getSchema() const = 0;

    /**
     * @brief 执行工具操作
     * @param args 工具参数 (JSON 格式)
     * @return 执行结果 (JSON 格式)
     * 
     * 返回格式应遵循 MCP 标准:
     * {
     *   "content": [
     *     {"type": "text", "text": "结果内容"}
     *   ]
     * }
     * 
     * 错误格式:
     * {
     *   "error": "错误描述"
     * }
     */
    virtual nlohmann::json execute(const nlohmann::json& args) = 0;
};
