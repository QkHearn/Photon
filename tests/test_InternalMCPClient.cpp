#include <gtest/gtest.h>
#include "mcp/InternalMCPClient.h"
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

        auto samplePath = (testDir / fs::path("sample.txt")).string();
        std::ofstream f(samplePath);
        f << "line1\nline2\n";
        f.close();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    fs::path testDir;
};

TEST_F(InternalMCPClientTest, ReadRequiresGrepSearch) {
    InternalMCPClient client(testDir.u8string());

    nlohmann::json readArgs = {
        {"path", "sample.txt"},
        {"start_line", 1},
        {"end_line", 1}
    };
    auto blocked = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(blocked).find("grep_search"), std::string::npos);

    client.callTool("grep_search", {{"pattern", "line1"}});
    auto readOk = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(readOk).find("line1"), std::string::npos);

    auto blockedAgain = client.callTool("read", readArgs);
    EXPECT_NE(ExtractText(blockedAgain).find("grep_search"), std::string::npos);
}

TEST_F(InternalMCPClientTest, WriteRequiresAuthorizationAndPriorRead) {
    InternalMCPClient client(testDir.u8string());

    nlohmann::json writeArgs = {
        {"path", "sample.txt"},
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

    client.callTool("grep_search", {{"pattern", "line1"}});
    client.callTool("read", {{"path", "sample.txt"}, {"start_line", 1}, {"end_line", 1}});

    auto writeOk = client.callTool("write", writeArgs);
    EXPECT_NE(ExtractText(writeOk).find("Replaced"), std::string::npos);

    auto samplePath = (testDir / fs::path("sample.txt")).string();
    std::ifstream f(samplePath);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    EXPECT_NE(content.find("updated"), std::string::npos);
}

TEST_F(InternalMCPClientTest, FullOverwriteBlockedForExistingFile) {
    InternalMCPClient client(testDir.u8string());
    client.callTool("authorize", nlohmann::json::object());

    client.callTool("grep_search", {{"pattern", "line1"}});
    client.callTool("read", {{"path", "sample.txt"}, {"start_line", 1}, {"end_line", 1}});

    nlohmann::json overwriteArgs = {
        {"path", "sample.txt"},
        {"content", "all new"}
    };
    auto blocked = client.callTool("write", overwriteArgs);
    EXPECT_NE(ExtractText(blocked).find("禁止对已存在文件进行整文件覆写"), std::string::npos);
}
