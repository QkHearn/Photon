#include <gtest/gtest.h>
#include "../src/ContextManager.h"
#include <memory>

// Mock LLMClient for testing context management without network
class MockLLMClient : public LLMClient {
public:
    MockLLMClient() : LLMClient("fake_key", "fake_url", "fake_model") {}
    std::string summarize(const std::string& text) override {
        return "summarized_content";
    }
    std::string chat(const std::string& prompt, const std::string& role) override {
        return "mock_response";
    }
};

TEST(ContextManagerTest, ThresholdCompression) {
    auto mockClient = std::make_shared<MockLLMClient>();
    // Set a small threshold
    ContextManager cm(mockClient, 50); 

    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", "You are a helpful assistant."}});
    messages.push_back({{"role", "user"}, {"content", "A very long message that will definitely exceed the fifty character threshold set for this test case."}});
    messages.push_back({{"role", "assistant"}, {"content", "OK"}});
    messages.push_back({{"role", "user"}, {"content", "Message 1"}});
    messages.push_back({{"role", "assistant"}, {"content", "Response 1"}});
    messages.push_back({{"role", "user"}, {"content", "Message 2"}});
    messages.push_back({{"role", "assistant"}, {"content", "Response 2"}});

    nlohmann::json result = cm.manage(messages);
    
    // Check if system prompt is preserved
    EXPECT_EQ(result[0]["role"], "system");
    EXPECT_EQ(result[0]["content"], "You are a helpful assistant.");
    
    // Check if summary is injected (middle part compressed)
    bool foundSummary = false;
    for (const auto& msg : result) {
        if (msg["role"] == "system" && msg["content"].get<std::string>().find("Summary") != std::string::npos) {
            foundSummary = true;
            break;
        }
    }
    EXPECT_TRUE(foundSummary);
    
    // Check if last message is preserved
    EXPECT_EQ(result.back()["role"], "assistant");
    EXPECT_EQ(result.back()["content"], "Response 2");
}

TEST(ContextManagerTest, ForceCompress) {
    auto mockClient = std::make_shared<MockLLMClient>();
    ContextManager cm(mockClient, 1000);
    
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", "sys"}});
    messages.push_back({{"role", "user"}, {"content", "msg1"}});
    messages.push_back({{"role", "assistant"}, {"content", "res1"}});
    messages.push_back({{"role", "user"}, {"content", "msg2"}});

    nlohmann::json result = cm.forceCompress(messages);
    
    EXPECT_EQ(result[0]["content"], "sys");
    EXPECT_TRUE(result[1]["content"].get<std::string>().find("Summary") != std::string::npos);
    EXPECT_EQ(result.back()["content"], "msg2");
}
