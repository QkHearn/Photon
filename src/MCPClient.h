#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdio>
#include <memory>

#ifdef _WIN32
    #include <io.h>
    #include <process.h>
    #define popen _popen
    #define pclose _pclose
    typedef int pid_t;
#else
    #include <unistd.h>
#endif

class IMCPClient {
public:
    virtual ~IMCPClient() = default;
    virtual nlohmann::json listTools() = 0;
    virtual nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments) = 0;
    virtual std::string getLastFile() const { return ""; }
};

class MCPClient : public IMCPClient {
public:
    MCPClient(const std::string& serverCommand);
    ~MCPClient() override;

    // 初始化 MCP 服务器
    bool initialize();

    // 列出资源
    nlohmann::json listResources();

    // 列出工具
    nlohmann::json listTools() override;

    // 读取资源
    std::string readResource(const std::string& uri);

    // 调用工具
    nlohmann::json callTool(const std::string& name, const nlohmann::json& arguments) override;

private:
    std::string serverCommand;
    int requestId = 0;
    int readPipe[2];
    int writePipe[2];
    pid_t childPid = -1;
    
    bool startProcess();
    void stopProcess();
    nlohmann::json sendRequest(const std::string& method, const nlohmann::json& params);
};
