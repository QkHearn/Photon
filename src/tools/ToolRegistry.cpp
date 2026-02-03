#include "ToolRegistry.h"

void ToolRegistry::registerTool(std::unique_ptr<ITool> tool) {
    if (!tool) return;
    
    std::string name = tool->getName();
    if (tools.count(name)) {
        // 工具已存在,覆盖 (可以考虑警告)
        // TODO: 添加日志
    }
    
    tools[name] = std::move(tool);
}

ITool* ToolRegistry::getTool(const std::string& name) {
    auto it = tools.find(name);
    if (it == tools.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<nlohmann::json> ToolRegistry::listToolSchemas() const {
    std::vector<nlohmann::json> schemas;
    
    for (const auto& [name, tool] : tools) {
        nlohmann::json schema;
        schema["type"] = "function";
        
        nlohmann::json function;
        function["name"] = tool->getName();
        function["description"] = tool->getDescription();
        function["parameters"] = tool->getSchema();
        
        schema["function"] = function;
        schemas.push_back(schema);
    }
    
    return schemas;
}

nlohmann::json ToolRegistry::executeTool(const std::string& name, const nlohmann::json& args) {
    ITool* tool = getTool(name);
    if (!tool) {
        nlohmann::json error;
        error["error"] = "Tool not found: " + name;
        return error;
    }
    
    try {
        return tool->execute(args);
    } catch (const std::exception& e) {
        nlohmann::json error;
        error["error"] = std::string("Tool execution failed: ") + e.what();
        return error;
    }
}

bool ToolRegistry::hasTool(const std::string& name) const {
    return tools.count(name) > 0;
}
