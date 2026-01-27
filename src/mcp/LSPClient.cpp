#include "mcp/LSPClient.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace {
std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string guessLanguageId(const std::string& path) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".cpp") return "cpp";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".hpp") return "cpp";
    if (path.size() >= 2 && path.substr(path.size() - 2) == ".h") return "cpp";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".py") return "python";
    return "plaintext";
}
} // namespace

LSPClient::LSPClient(const std::string& serverPath, const std::string& rootUri)
    : serverPath(serverPath), rootUri(rootUri) {}

LSPClient::~LSPClient() {
    stopProcess();
}

bool LSPClient::initialize() {
    if (initialized) return true;
    if (serverPath.empty()) return false;
#ifdef _WIN32
    return false;
#else
    if (!startProcess()) return false;

    nlohmann::json params = {
        {"processId", static_cast<int>(getpid())},
        {"rootUri", rootUri},
        {"capabilities", nlohmann::json::object()}
    };
    auto result = sendRequest("initialize", params);
    (void)result;
    sendNotification("initialized", nlohmann::json::object());
    initialized = true;
    return true;
#endif
}

std::vector<LSPClient::Location> LSPClient::goToDefinition(const std::string& fileUri, const Position& position) {
    if (!initialized && !initialize()) return {};
    if (!ensureDocumentOpen(fileUri)) return {};
    nlohmann::json params = {
        {"textDocument", {{"uri", fileUri}}},
        {"position", {{"line", position.line}, {"character", position.character}}}
    };
    auto result = sendRequest("textDocument/definition", params);
    if (lastRequestTimedOut) {
        stopProcess();
        initialized = false;
        if (initialize()) {
            result = sendRequest("textDocument/definition", params);
        }
    }
    return parseLocations(result);
}

std::vector<LSPClient::Location> LSPClient::findReferences(const std::string& fileUri, const Position& position) {
    if (!initialized && !initialize()) return {};
    if (!ensureDocumentOpen(fileUri)) return {};
    nlohmann::json params = {
        {"textDocument", {{"uri", fileUri}}},
        {"position", {{"line", position.line}, {"character", position.character}}},
        {"context", {{"includeDeclaration", true}}}
    };
    auto result = sendRequest("textDocument/references", params);
    if (lastRequestTimedOut) {
        stopProcess();
        initialized = false;
        if (initialize()) {
            result = sendRequest("textDocument/references", params);
        }
    }
    return parseLocations(result);
}

bool LSPClient::startProcess() {
#ifdef _WIN32
    return false;
#else
    if (pid > 0) return true;
    auto args = splitArgs(serverPath);
    if (args.empty()) return false;

    int inPipe[2];
    int outPipe[2];
    if (pipe(inPipe) != 0) return false;
    if (pipe(outPipe) != 0) {
        close(inPipe[0]);
        close(inPipe[1]);
        return false;
    }

    pid = fork();
    if (pid == 0) {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            dup2(devNull, STDERR_FILENO);
        }
        close(inPipe[1]);
        close(outPipe[0]);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(1);
    }

    if (pid < 0) {
        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);
        return false;
    }

    close(inPipe[0]);
    close(outPipe[1]);
    writeFd = inPipe[1];
    readFd = outPipe[0];
    stopReader = false;
    readerThread = std::thread(&LSPClient::readerLoop, this);
    return true;
#endif
}

void LSPClient::stopProcess() {
#ifdef _WIN32
    return;
#else
    if (pid <= 0) return;
    stopReader = true;
    if (readFd >= 0) close(readFd);
    if (writeFd >= 0) close(writeFd);
    if (readerThread.joinable()) readerThread.join();
    kill(pid, SIGTERM);
    int status = 0;
    waitpid(pid, &status, 0);
    pid = -1;
#endif
}

void LSPClient::readerLoop() {
#ifdef _WIN32
    return;
#else
    std::string buffer;
    char temp[4096];
    while (!stopReader) {
        ssize_t n = read(readFd, temp, sizeof(temp));
        if (n <= 0) break;
        buffer.append(temp, temp + n);

        while (true) {
            auto headerEnd = buffer.find("\r\n\r\n");
            if (headerEnd == std::string::npos) break;

            std::string header = buffer.substr(0, headerEnd);
            std::string headerLower = toLower(header);
            std::size_t pos = headerLower.find("content-length:");
            if (pos == std::string::npos) {
                buffer.erase(0, headerEnd + 4);
                continue;
            }

            std::size_t lineEnd = headerLower.find("\r\n", pos);
            std::string lenStr = header.substr(pos + 15, lineEnd - (pos + 15));
            int length = std::stoi(lenStr);
            std::size_t bodyStart = headerEnd + 4;
            if (buffer.size() < bodyStart + static_cast<std::size_t>(length)) break;

            std::string body = buffer.substr(bodyStart, length);
            buffer.erase(0, bodyStart + length);

            try {
                auto msg = nlohmann::json::parse(body);
                if (msg.contains("id")) {
                    int id = msg["id"].get<int>();
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        responses[id] = msg;
                    }
                    cv.notify_all();
                }
            } catch (...) {
            }
        }
    }
#endif
}

bool LSPClient::sendMessage(const nlohmann::json& msg) {
#ifdef _WIN32
    (void)msg;
    return false;
#else
    if (writeFd < 0) return false;
    std::string payload = msg.dump();
    std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
    std::string full = header + payload;
    ssize_t total = 0;
    const char* data = full.c_str();
    ssize_t len = static_cast<ssize_t>(full.size());
    while (total < len) {
        ssize_t n = write(writeFd, data + total, len - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
#endif
}

nlohmann::json LSPClient::sendRequest(const std::string& method, const nlohmann::json& params) {
    int id = 0;
    {
        std::lock_guard<std::mutex> lock(mtx);
        id = ++requestId;
    }
    nlohmann::json msg = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", method},
        {"params", params}
    };
    if (!sendMessage(msg)) return nlohmann::json::object();

    std::unique_lock<std::mutex> lock(mtx);
    lastRequestTimedOut = false;
    bool ok = cv.wait_for(lock, std::chrono::seconds(10), [&] { return responses.find(id) != responses.end(); });
    if (!ok) {
        lastRequestTimedOut = true;
        return nlohmann::json::object();
    }
    auto resp = responses[id];
    responses.erase(id);
    if (resp.contains("result")) return resp["result"];
    return resp;
}

void LSPClient::sendNotification(const std::string& method, const nlohmann::json& params) {
    nlohmann::json msg = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    sendMessage(msg);
}

bool LSPClient::ensureDocumentOpen(const std::string& fileUri) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (openedDocuments.find(fileUri) != openedDocuments.end()) return true;
    }
    std::string path = uriToPath(fileUri);
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    nlohmann::json params = {
        {"textDocument", {
            {"uri", fileUri},
            {"languageId", guessLanguageId(path)},
            {"version", 1},
            {"text", content}
        }}
    };
    sendNotification("textDocument/didOpen", params);
    {
        std::lock_guard<std::mutex> lock(mtx);
        openedDocuments.insert(fileUri);
    }
    return true;
}

std::vector<LSPClient::Location> LSPClient::parseLocations(const nlohmann::json& result) {
    std::vector<Location> locations;
    if (result.is_null()) return locations;

    auto addLocation = [&](const nlohmann::json& item) {
        Location loc;
        if (item.contains("uri")) {
            loc.uri = item["uri"].get<std::string>();
            if (item.contains("range")) {
                loc.range.start.line = item["range"]["start"]["line"].get<int>();
                loc.range.start.character = item["range"]["start"]["character"].get<int>();
                loc.range.end.line = item["range"]["end"]["line"].get<int>();
                loc.range.end.character = item["range"]["end"]["character"].get<int>();
            }
            locations.push_back(loc);
        } else if (item.contains("targetUri")) {
            loc.uri = item["targetUri"].get<std::string>();
            if (item.contains("targetRange")) {
                loc.range.start.line = item["targetRange"]["start"]["line"].get<int>();
                loc.range.start.character = item["targetRange"]["start"]["character"].get<int>();
                loc.range.end.line = item["targetRange"]["end"]["line"].get<int>();
                loc.range.end.character = item["targetRange"]["end"]["character"].get<int>();
            }
            locations.push_back(loc);
        }
    };

    if (result.is_array()) {
        for (const auto& item : result) addLocation(item);
    } else if (result.is_object()) {
        addLocation(result);
    }
    return locations;
}

std::string LSPClient::uriToPath(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.rfind(prefix, 0) == 0) return uri.substr(prefix.size());
    return uri;
}

std::vector<std::string> LSPClient::splitArgs(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::vector<std::string> out;
    std::string token;
    while (iss >> token) {
        out.push_back(token);
    }
    return out;
}
