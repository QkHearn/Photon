#include "mcp/MCPClient.h"
#include <sstream>
#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#else
#include <windows.h>
#endif

MCPClient::MCPClient(const std::string& command) : serverCommand(command) {}

MCPClient::~MCPClient() {
    stopProcess();
}

#ifndef _WIN32
// POSIX Implementation
bool MCPClient::startProcess() {
    if (pipe(readPipe) == -1 || pipe(writePipe) == -1) {
        return false;
    }

    childPid = fork();
    if (childPid == -1) {
        return false;
    }

    if (childPid == 0) { // Child
        dup2(writePipe[0], STDIN_FILENO);
        dup2(readPipe[1], STDOUT_FILENO);
        
        close(writePipe[1]);
        close(readPipe[0]);
        
        execl("/bin/sh", "sh", "-c", serverCommand.c_str(), (char*)NULL);
        _exit(1);
    } else { // Parent
        close(writePipe[0]);
        close(readPipe[1]);
        return true;
    }
}

void MCPClient::stopProcess() {
    if (childPid != -1) {
        close(writePipe[1]);
        close(readPipe[0]);
        kill(childPid, SIGTERM);
        waitpid(childPid, NULL, 0);
        childPid = -1;
    }
}

nlohmann::json MCPClient::sendRequest(const std::string& method, const nlohmann::json& params) {
    if (childPid == -1) {
        if (!startProcess()) {
            return {{"error", "Failed to start process"}};
        }
    }

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", ++requestId},
        {"method", method},
        {"params", params}
    };

    std::string reqStr = request.dump() + "\n";
    if (write(writePipe[1], reqStr.c_str(), reqStr.length()) <= 0) return nlohmann::json::object();

    std::string responseLine;
    char buffer[1];
    
    // Read until we find a line that looks like a JSON object (starts with {)
    while (true) {
        responseLine.clear();
        ssize_t bytesRead;
        while ((bytesRead = read(readPipe[0], buffer, 1)) > 0) {
            if (buffer[0] == '\n') break;
            responseLine += buffer[0];
        }
        
        if (bytesRead <= 0 && responseLine.empty()) {
            return nlohmann::json::object();
        }
        
        // Skip potential non-JSON noise (like logs or warnings)
        if (!responseLine.empty() && responseLine[0] == '{') {
            try {
                return nlohmann::json::parse(responseLine);
            } catch (...) {
                // Not valid JSON, continue to next line
            }
        }
        
        if (bytesRead <= 0) break;
    }
    return nlohmann::json::object();
}
#else
// Windows Stubs (To allow building, full implementation requires CreateProcess/Pipe)
bool MCPClient::startProcess() {
    std::cerr << "External MCP servers via stdio not yet fully implemented on Windows." << std::endl;
    return false;
}
void MCPClient::stopProcess() {}
nlohmann::json MCPClient::sendRequest(const std::string& method, const nlohmann::json& params) {
    return {{"error", "External MCP not supported on Windows yet"}};
}
#endif

bool MCPClient::initialize() {
    nlohmann::json params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", nlohmann::json::object()},
        {"clientInfo", {{"name", "Photon-Agent-CPP"}, {"version", "1.0.0"}}}
    };
    auto res = sendRequest("initialize", params);
    
    // Send notifications if needed (initialized)
    nlohmann::json notification = {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"},
        {"params", {}}
    };
    std::string reqStr = notification.dump() + "\n";
#ifdef _WIN32
    auto ignored = _write(writePipe[1], reqStr.c_str(), static_cast<unsigned int>(reqStr.length()));
    (void)ignored;
#else
    auto ignored = write(writePipe[1], reqStr.c_str(), reqStr.length());
    (void)ignored;
#endif

    return !res.is_null() && res.contains("result");
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
    auto res = sendRequest("tools/call", {{"name", name}, {"arguments", arguments}});
    if (res.contains("result")) {
        return res["result"];
    }
    return res;
}
