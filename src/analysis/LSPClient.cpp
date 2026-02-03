#include "analysis/LSPClient.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <cctype>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace fs = std::filesystem;

namespace {
std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string guessLanguageId(const std::string& path) {
    std::string ext = toLower(fs::path(path).extension().string());
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") return "cpp";
    if (ext == ".py") return "python";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".js" || ext == ".jsx") return "javascript";
    if (ext == ".ets") return "arkts";
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

    if (!startProcess()) {
        return false;
    }

    nlohmann::json params = {
        {"processId", 
#ifdef _WIN32
            static_cast<int>(GetCurrentProcessId())
#else
            static_cast<int>(getpid())
#endif
        },
        {"rootUri", rootUri},
        {"capabilities", {
            {"textDocument", {
                {"documentSymbol", {{"hierarchicalDocumentSymbolSupport", true}}}
            }}
        }}
    };
    
    auto result = sendRequest("initialize", params);
    (void)result;
    
    sendNotification("initialized", nlohmann::json::object());
    initialized = true;
    return true;
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

std::vector<LSPClient::DocumentSymbol> LSPClient::documentSymbols(const std::string& fileUri) {
    if (!initialized && !initialize()) return {};
    if (!ensureDocumentOpen(fileUri)) return {};
    nlohmann::json params = {
        {"textDocument", {{"uri", fileUri}}}
    };
    auto result = sendRequest("textDocument/documentSymbol", params);
    if (lastRequestTimedOut) {
        stopProcess();
        initialized = false;
        if (initialize()) {
            result = sendRequest("textDocument/documentSymbol", params);
        }
    }
    return parseDocumentSymbols(result);
}

bool LSPClient::startProcess() {
#ifdef _WIN32
    if (processHandle) return true;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return false;
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return false;

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.hStdInput = hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd = "cmd.exe /c " + serverPath;
    if (!CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
        CloseHandle(hChildStd_IN_Rd);
        CloseHandle(hChildStd_IN_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return false;
    }

    processHandle = piProcInfo.hProcess;
    threadHandle = piProcInfo.hThread;
    stdinWrite = hChildStd_IN_Wr;
    stdoutRead = hChildStd_OUT_Rd;

    CloseHandle(hChildStd_IN_Rd);
    CloseHandle(hChildStd_OUT_Wr);

    stopReader = false;
    readerThread = std::thread(&LSPClient::readerLoop, this);
    return true;
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
    if (!processHandle) return;
    stopReader = true;
    if (stdinWrite) CloseHandle(static_cast<HANDLE>(stdinWrite));
    if (stdoutRead) CloseHandle(static_cast<HANDLE>(stdoutRead));
    if (readerThread.joinable()) readerThread.join();
    TerminateProcess(static_cast<HANDLE>(processHandle), 0);
    CloseHandle(static_cast<HANDLE>(processHandle));
    CloseHandle(static_cast<HANDLE>(threadHandle));
    processHandle = nullptr;
    threadHandle = nullptr;
    stdinWrite = nullptr;
    stdoutRead = nullptr;
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
    std::string buffer;
    char temp[4096];
    while (!stopReader) {
#ifdef _WIN32
        DWORD n = 0;
        if (!ReadFile(static_cast<HANDLE>(stdoutRead), temp, sizeof(temp), &n, NULL) || n == 0) break;
#else
        ssize_t n = read(readFd, temp, sizeof(temp));
        if (n <= 0) break;
#endif
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
}

bool LSPClient::sendMessage(const nlohmann::json& msg) {
    std::string payload = msg.dump();
    std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
    std::string full = header + payload;
    const char* data = full.c_str();
    size_t len = full.size();

#ifdef _WIN32
    if (!stdinWrite) return false;
    DWORD total = 0;
    while (total < len) {
        DWORD n = 0;
        if (!WriteFile(static_cast<HANDLE>(stdinWrite), data + total, static_cast<DWORD>(len - total), &n, NULL)) return false;
        total += n;
    }
    return true;
#else
    if (writeFd < 0) return false;
    ssize_t total = 0;
    while (total < static_cast<ssize_t>(len)) {
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

std::vector<LSPClient::DocumentSymbol> LSPClient::parseDocumentSymbols(const nlohmann::json& result) {
    std::vector<DocumentSymbol> symbols;
    if (result.is_null()) return symbols;

    auto parseRange = [](const nlohmann::json& j, Range& r) -> bool {
        if (!j.is_object()) return false;
        if (!j.contains("start") || !j.contains("end")) return false;
        const auto& s = j["start"];
        const auto& e = j["end"];
        if (!s.is_object() || !e.is_object()) return false;
        r.start.line = s.value("line", 0);
        r.start.character = s.value("character", 0);
        r.end.line = e.value("line", 0);
        r.end.character = e.value("character", 0);
        return true;
    };

    auto flatten = [&](const DocumentSymbol& sym, auto& self, std::vector<DocumentSymbol>& out) -> void {
        DocumentSymbol copy = sym;
        copy.children.clear();
        out.push_back(std::move(copy));
        for (const auto& child : sym.children) {
            self(child, self, out);
        }
    };

    auto parseDocumentSymbol = [&](const nlohmann::json& item, auto& self) -> DocumentSymbol {
        DocumentSymbol sym;
        sym.name = item.value("name", "");
        sym.kind = item.value("kind", 0);
        sym.detail = item.value("detail", "");
        if (item.contains("range")) parseRange(item["range"], sym.range);
        if (item.contains("selectionRange")) parseRange(item["selectionRange"], sym.selectionRange);
        if (item.contains("children") && item["children"].is_array()) {
            for (const auto& child : item["children"]) {
                sym.children.push_back(self(child, self));
            }
        }
        return sym;
    };

    auto parseSymbolInformation = [&](const nlohmann::json& item) -> std::optional<DocumentSymbol> {
        if (!item.contains("name") || !item.contains("kind") || !item.contains("location")) return std::nullopt;
        const auto& loc = item["location"];
        if (!loc.contains("range")) return std::nullopt;
        DocumentSymbol sym;
        sym.name = item.value("name", "");
        sym.kind = item.value("kind", 0);
        if (item.contains("containerName")) {
            sym.detail = item.value("containerName", "");
        }
        parseRange(loc["range"], sym.range);
        sym.selectionRange = sym.range;
        return sym;
    };

    if (result.is_array()) {
        for (const auto& item : result) {
            if (item.contains("location")) {
                auto sym = parseSymbolInformation(item);
                if (sym.has_value()) symbols.push_back(sym.value());
                continue;
            }
            if (item.contains("name")) {
                auto sym = parseDocumentSymbol(item, parseDocumentSymbol);
                flatten(sym, flatten, symbols);
            }
        }
    } else if (result.is_object()) {
        if (result.contains("location")) {
            auto sym = parseSymbolInformation(result);
            if (sym.has_value()) symbols.push_back(sym.value());
        } else if (result.contains("name")) {
            auto sym = parseDocumentSymbol(result, parseDocumentSymbol);
            flatten(sym, flatten, symbols);
        }
    }
    return symbols;
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
