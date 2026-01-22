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
    // Set a very small threshold to trigger compression quickly
    ContextManager cm(mockClient, 20); 

    cm.addContext("This is a very long string that should exceed twenty chars");
    
    std::string context = cm.getContext();
    // Should contain the summary prefix from our mock
    EXPECT_TRUE(context.find("Previous Context Summary: summarized_content") != std::string::npos);
}

TEST(ContextManagerTest, ManualClear) {
    auto mockClient = std::make_shared<MockLLMClient>();
    ContextManager cm(mockClient, 1000);
    cm.addContext("some info");
    cm.clear();
    EXPECT_EQ(cm.getContext(), "");
}
