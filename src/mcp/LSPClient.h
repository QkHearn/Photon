#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#ifndef _WIN32
#include <sys/types.h>
#endif

class LSPClient {
public:
    struct Position {
        int line;
        int character;
    };

    struct Range {
        Position start;
        Position end;
    };

    struct Location {
        std::string uri;
        Range range;
    };

    LSPClient(const std::string& serverPath, const std::string& rootUri);
    ~LSPClient();

    bool initialize();
    std::vector<Location> goToDefinition(const std::string& fileUri, const Position& position);
    std::vector<Location> findReferences(const std::string& fileUri, const Position& position);

private:
    std::string serverPath;
    std::string rootUri;
    int requestId = 0;
    bool initialized = false;

#ifdef _WIN32
    void* processHandle = nullptr;
    void* threadHandle = nullptr;
    void* stdinWrite = nullptr;
    void* stdoutRead = nullptr;
#else
    pid_t pid = -1;
    int readFd = -1;
    int writeFd = -1;
#endif

    std::thread readerThread;
    bool stopReader = false;

    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_map<int, nlohmann::json> responses;
    std::unordered_set<std::string> openedDocuments;
    std::atomic<bool> lastRequestTimedOut{false};

    bool startProcess();
    void stopProcess();
    void readerLoop();
    bool sendMessage(const nlohmann::json& msg);
    nlohmann::json sendRequest(const std::string& method, const nlohmann::json& params);
    void sendNotification(const std::string& method, const nlohmann::json& params);
    bool ensureDocumentOpen(const std::string& fileUri);
    std::vector<Location> parseLocations(const nlohmann::json& result);
    static std::string uriToPath(const std::string& uri);
    static std::vector<std::string> splitArgs(const std::string& cmd);
};
