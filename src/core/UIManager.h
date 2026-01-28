#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <functional>
#include "utils/Logger.h"

class UIManager {
public:
    static UIManager& getInstance() {
        static UIManager instance;
        return instance;
    }

    enum class Mode {
        CLI
    };

    using InputCallback = std::function<void(const std::string&)>;

    void start(InputCallback onInput);
    void stop();
    
    void setMode(Mode mode);
    Mode getMode() const { return Mode::CLI; }

    // Data update methods (kept for compatibility, though some may do nothing in CLI)
    void addThought(const std::string& thought);
    void addAction(const std::string& action);
    void addChatMessage(const std::string& role, const std::string& content);
    void addSystemLog(const std::string& log, LogLevel level = LogLevel::INFO);
    void updateStatus(const std::string& model, int tokens, int tasks);
    void setDiff(const std::string& diff);
    void appendToLastThought(const std::string& delta);
    void appendToLastChat(const std::string& delta);

private:
    UIManager();
    ~UIManager();

    void setupLogger();

    std::atomic<bool> running{false};
    InputCallback inputCallback;
};
