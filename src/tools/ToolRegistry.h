#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include "ITool.h"

/**
 * @brief 工具注册中心
 * 
 * 统一管理所有工具的注册、查找和执行。
 * 这是工具层的唯一入口。
 */
class ToolRegistry {
public:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    /**
     * @brief 注册一个工具
     * @param tool 工具实例 (unique_ptr 转移所有权)
     */
    void registerTool(std::unique_ptr<ITool> tool);

    /**
     * @brief 获取工具实例
     * @param name 工具名称
     * @return 工具指针 (如果不存在返回 nullptr)
     */
    ITool* getTool(const std::string& name);

    /**
     * @brief 列出所有工具的 Schema
     * @return JSON 数组,每个元素包含工具的完整定义
     * 
     * 格式:
     * [
     *   {
     *     "type": "function",
     *     "function": {
     *       "name": "tool_name",
     *       "description": "工具描述",
     *       "parameters": { JSON Schema }
     *     }
     *   }
     * ]
     */
    std::vector<nlohmann::json> listToolSchemas() const;

    /**
     * @brief 执行工具
     * @param name 工具名称
     * @param args 工具参数
     * @return 执行结果
     * 
     * 如果工具不存在,返回:
     * {"error": "Tool not found: xxx"}
     */
    nlohmann::json executeTool(const std::string& name, const nlohmann::json& args);

    /**
     * @brief 获取已注册工具数量
     */
    size_t getToolCount() const { return tools.size(); }

    /**
     * @brief 检查工具是否已注册
     */
    bool hasTool(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> tools;
};
