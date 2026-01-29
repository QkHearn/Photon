#include <gtest/gtest.h>
#include "mcp/InternalMCPClient.h"
#include "utils/SymbolManager.h"
#include "utils/RegexSymbolProvider.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {
std::string ExtractText(const nlohmann::json& res) {
    if (res.contains("content") && res["content"].is_array() && !res["content"].empty()) {
        const auto& item = res["content"][0];
        if (item.contains("text")) return item["text"].get<std::string>();
    }
    return "";
}
} // namespace

class InternalMCPClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto folderName = std::string("photon_mcp_test_") + std::to_string(now);
        testDir = fs::temp_directory_path() / fs::path(folderName);
        fs::create_directories(testDir);

        auto samplePath = (testDir / fs::path("sample.cpp")).string();
        std::ofstream f(samplePath);
        f << "int sample() { return 1; }\n";
        f.close();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    fs::path testDir;
};

TEST_F(InternalMCPClientTest, ReadRequiresContextPlan) {
    InternalMCPClient client(testDir.u8string());

    SymbolManager symbolMgr(testDir.u8string());
    symbolMgr.registerProvider(std::make_unique<RegexSymbolProvider>());

    auto codePath = (testDir / fs::path("main.cpp")).string();
    std::ofstream f(codePath);
    f << "int main() { return 0; }\n";
    f.close();
    symbolMgr.updateFile("main.cpp");

    client.setSymbolManager(&symbolMgr);

    nlohmann::json readArgs = {
        {"path", "main.cpp"},
        {"start_line", 1},
        {"end_line", 1}
    };
    auto blocked = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(blocked).find("context_plan"), std::string::npos);

    client.callTool("context_plan", {{"query", "main"}});
    auto readOk = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(readOk).find("结构化压缩器"), std::string::npos);
}

TEST_F(InternalMCPClientTest, WriteRequiresAuthorizationAndPriorRead) {
    InternalMCPClient client(testDir.u8string());

    SymbolManager symbolMgr(testDir.u8string());
    symbolMgr.registerProvider(std::make_unique<RegexSymbolProvider>());
    symbolMgr.updateFile("sample.cpp");
    client.setSymbolManager(&symbolMgr);

    nlohmann::json writeArgs = {
        {"path", "sample.cpp"},
        {"operation", "replace"},
        {"start_line", 1},
        {"end_line", 1},
        {"content", "updated"}
    };

    auto needAuth = client.callTool("write", writeArgs);
    EXPECT_EQ(needAuth.value("status", ""), "requires_confirmation");

    client.callTool("authorize", nlohmann::json::object());
    auto needRead = client.callTool("write", writeArgs);
    EXPECT_NE(ExtractText(needRead).find("写入前请先 read"), std::string::npos);

    client.callTool("context_plan", {{"query", "sample"}});
    client.callTool("read", {{"path", "sample.cpp"}, {"start_line", 1}, {"end_line", 1}});

    auto writeOk = client.callTool("write", writeArgs);
    EXPECT_NE(ExtractText(writeOk).find("Replaced"), std::string::npos);

    auto samplePath = (testDir / fs::path("sample.cpp")).string();
    std::ifstream f(samplePath);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    EXPECT_NE(content.find("updated"), std::string::npos);
}

TEST_F(InternalMCPClientTest, FullOverwriteBlockedForExistingFile) {
    InternalMCPClient client(testDir.u8string());
    client.callTool("authorize", nlohmann::json::object());

    SymbolManager symbolMgr(testDir.u8string());
    symbolMgr.registerProvider(std::make_unique<RegexSymbolProvider>());
    symbolMgr.updateFile("sample.cpp");
    client.setSymbolManager(&symbolMgr);

    client.callTool("context_plan", {{"query", "sample"}});
    client.callTool("read", {{"path", "sample.cpp"}, {"start_line", 1}, {"end_line", 1}});

    nlohmann::json overwriteArgs = {
        {"path", "sample.cpp"},
        {"content", "all new"}
    };
    auto blocked = client.callTool("write", overwriteArgs);
    EXPECT_NE(ExtractText(blocked).find("禁止对已存在文件进行整文件覆写"), std::string::npos);
}

TEST_F(InternalMCPClientTest, ReadIncludeRawAndCompressFlags) {
    InternalMCPClient client(testDir.u8string());

    SymbolManager symbolMgr(testDir.u8string());
    symbolMgr.registerProvider(std::make_unique<RegexSymbolProvider>());
    auto codePath = (testDir / fs::path("main.cpp")).string();
    std::ofstream f(codePath);
    f << "int main() { return 0; }\n";
    f.close();
    symbolMgr.updateFile("main.cpp");
    client.setSymbolManager(&symbolMgr);

    client.callTool("context_plan", {{"query", "main"}});
    nlohmann::json readArgs = {
        {"path", "main.cpp"},
        {"start_line", 1},
        {"end_line", 1}
    };
    auto compressed = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(compressed).find("压缩视图"), std::string::npos);

    nlohmann::json rawArgs = {
        {"path", "main.cpp"},
        {"start_line", 1},
        {"end_line", 1},
        {"compress", false},
        {"include_raw", true}
    };
    auto raw = client.callTool("read", rawArgs);
    EXPECT_NE(ExtractText(raw).find("int main()"), std::string::npos);
}
