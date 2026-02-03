#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @brief Agent 状态管理
 * 
 * 记录当前任务的执行状态,包括目标、阶段、完成步骤、失败尝试等。
 */
struct AgentState {
    /**
     * @brief 当前任务的目标 (用户输入)
     */
    std::string taskGoal;

    /**
     * @brief 当前执行阶段
     * 可选值: "planning", "acting", "observing", "completed", "failed"
     */
    std::string currentPhase = "planning";

    /**
     * @brief 当前迭代次数
     */
    int iteration = 0;

    /**
     * @brief 是否已完成
     */
    bool isComplete = false;

    /**
     * @brief 已完成的步骤列表
     * 每个元素是一个简短的步骤描述
     */
    std::vector<std::string> completedSteps;

    /**
     * @brief 失败尝试列表
     * 记录哪些操作失败了,用于避免重复犯错
     * 格式: {"tool": "xxx", "args": {...}, "error": "xxx"}
     */
    std::vector<nlohmann::json> failedAttempts;

    /**
     * @brief 计划中的操作列表
     * LLM 生成的工具调用计划
     */
    std::vector<nlohmann::json> plannedActions;

    /**
     * @brief 观察结果
     * 上一次执行的结果
     */
    std::vector<nlohmann::json> observations;

    /**
     * @brief 上下文数据
     * 存储任务相关的临时数据 (不是长期记忆)
     */
    nlohmann::json context = nlohmann::json::object();

    /**
     * @brief 重置状态 (用于新任务)
     */
    void reset() {
        taskGoal.clear();
        currentPhase = "planning";
        iteration = 0;
        isComplete = false;
        completedSteps.clear();
        failedAttempts.clear();
        plannedActions.clear();
        observations.clear();
        context = nlohmann::json::object();
    }

    /**
     * @brief 添加完成步骤
     */
    void addCompletedStep(const std::string& step) {
        completedSteps.push_back(step);
    }

    /**
     * @brief 记录失败尝试
     */
    void recordFailure(const std::string& tool, const nlohmann::json& args, const std::string& error) {
        nlohmann::json failure;
        failure["tool"] = tool;
        failure["args"] = args;
        failure["error"] = error;
        failedAttempts.push_back(failure);
    }

    /**
     * @brief 检查是否有类似失败
     * 简单检查: 工具名和错误信息相似
     */
    bool hasSimilarFailure(const std::string& tool, const std::string& error) const {
        for (const auto& failure : failedAttempts) {
            if (failure["tool"] == tool && 
                failure["error"].get<std::string>().find(error) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};
