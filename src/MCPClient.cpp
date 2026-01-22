#include "MCPClient.h"
#include <sstream>

MCPClient::MCPClient(const std::string& command) : serverCommand(command) {}

MCPClient::~MCPClient() {}

nlohmann::json MCPClient::sendRequest(const std::string& method, const nlohmann::json& params) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", ++requestId},
        {"method", method},
        {"params", params}
    };

    // 演示用途：由于 C++ 标准库不直接支持双向管道 popen，
    // 实际实现通常需要 fork/execve 或第三方库。
    // 这里展示 JSON-RPC 的封装逻辑。
    std::cout << "[MCP Debug] Sending Request: " << request.dump() << std::endl;
    
    // 模拟返回（实际应从服务器标准输出读取）
    return {{"result", {{"status", "connected"}}}};
}

bool MCPClient::initialize() {
    nlohmann::json params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {}},
        {"clientInfo", {{"name", "Photon-Agent-CPP"}, {"version", "1.0.0"}}}
    };
    auto res = sendRequest("initialize", params);
    return !res.is_null();
}

nlohmann::json MCPClient::listResources() {
    return sendRequest("resources/list", nlohmann::json::object());
}

nlohmann::json MCPClient::listTools() {
    return sendRequest("tools/list", nlohmann::json::object());
}

std::string MCPClient::readResource(const std::string& uri) {
    auto res = sendRequest("resources/read", {{"uri", uri}});
    if (res.contains("result") && res["result"].contains("contents")) {
        return res["result"]["contents"][0]["text"];
    }
    return "";
}

nlohmann::json MCPClient::callTool(const std::string& name, const nlohmann::json& arguments) {
    return sendRequest("tools/call", {{"name", name}, {"arguments", arguments}});
}
