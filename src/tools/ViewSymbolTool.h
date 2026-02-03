#pragma once
#include "ITool.h"
#include "analysis/SymbolManager.h"
#include <memory>

/**
 * @brief 查看特定符号的代码工具
 * 
 * 当 Agent 提供了文件的符号摘要后，LLM 可以使用此工具
 * 请求查看特定符号（函数、类等）的代码，而不是读取整个文件。
 */
class ViewSymbolTool : public ITool {
public:
    explicit ViewSymbolTool(SymbolManager* symbolMgr);
    
    std::string getName() const override;
    std::string getDescription() const override;
    nlohmann::json getSchema() const override;
    nlohmann::json execute(const nlohmann::json& args) override;

private:
    SymbolManager* symbolMgr;
    std::string rootPath;
};
