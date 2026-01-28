#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <vector>

enum class LogLevel {
    THOUGHT,
    ACTION,
    INFO,
    SUCCESS,
    WARNING,
    ERROR,
    DEBUG
};

class Logger {
public:
    using LogCallback = std::function<void(LogLevel, const std::string&)>;

    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setCallback(LogCallback callback) {
        std::lock_guard<std::mutex> lock(mtx);
        this->callback = callback;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        // Always log to file/console as fallback/persistence
        printToConsole(level, message);
        
        if (callback) {
            callback(level, message);
        }
    }

    // Convenience methods
    void thought(const std::string& m) { log(LogLevel::THOUGHT, m); }
    void action(const std::string& m) { log(LogLevel::ACTION, m); }
    void info(const std::string& m) { log(LogLevel::INFO, m); }
    void success(const std::string& m) { log(LogLevel::SUCCESS, m); }
    void warn(const std::string& m) { log(LogLevel::WARNING, m); }
    void error(const std::string& m) { log(LogLevel::ERROR, m); }

private:
    Logger() = default;
    LogCallback callback;
    std::mutex mtx;

    void printToConsole(LogLevel level, const std::string& message);
};
