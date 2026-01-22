#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdio>
#include <memory>

#ifdef _WIN32
    #include <io.h>
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
#endif

class MCPClient {
public:
    MCPClient(const std::string& serverCommand);
    ~MCPClient();

    // 初始化 MCP 服务器
    bool initialize();

    // 列出资源
    nlohmann::json listResources();

    // 列出工具
    nlohmann::json listTools();

    // 读取资源
    std::string readResource(const std::string& uri);

    // 调用工具
    nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments);

private:
    std::string serverCommand;
    int requestId = 0;
    
    nlohmann::json sendRequest(const std::string& method, const nlohmann::json& params);
    
    // 注意：在实际复杂的 C++ 生产环境中，需要使用更健壮的异步 IPC（如 boost::process 或 uvw）
    // 这里使用基础的 popen/stdio 演示 MCP 的 JSON-RPC 交互原理
};
