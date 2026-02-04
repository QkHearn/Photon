#include "utils/Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

// ANSI Color Codes (Reusing from main.cpp logic)
namespace {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string RED = "\033[38;5;196m";
    const std::string GREEN = "\033[38;5;46m";
    const std::string YELLOW = "\033[38;5;226m";
    const std::string BLUE = "\033[38;5;33m";
    const std::string MAGENTA = "\033[38;5;201m";
    const std::string CYAN = "\033[38;5;51m";
    const std::string GRAY = "\033[38;5;242m";
}

void Logger::printToConsole(LogLevel level, const std::string& message) {
    // 1. Always log to file for debugging
    std::ofstream logFile("photon.log", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        logFile << std::put_time(std::localtime(&now), "[%Y-%m-%d %H:%M:%S] ");
        switch (level) {
            case LogLevel::ERROR: logFile << "[ERROR] "; break;
            case LogLevel::WARNING: logFile << "[WARN] "; break;
            case LogLevel::INFO: logFile << "[INFO] "; break;
            case LogLevel::THOUGHT: logFile << "[THOUGHT] "; break;
            case LogLevel::ACTION: logFile << "[ACTION] "; break;
            default: logFile << "[DEBUG] "; break;
        }
        logFile << message << std::endl;
    }

    // 3. Simple prefix without rigid alignment
    std::string prefix;
    switch (level) {
        case LogLevel::THOUGHT:
            prefix = GRAY + BOLD + "🤔 [Think] " + RESET;
            break;
        case LogLevel::ACTION:
            prefix = YELLOW + BOLD + "⚙️ [Action] " + RESET;
            break;
        case LogLevel::INFO:
            prefix = CYAN + "[Info] " + RESET;
            break;
        case LogLevel::SUCCESS:
            prefix = GREEN + "✔ " + RESET;
            break;
        case LogLevel::WARNING:
            prefix = YELLOW + "⚠ " + RESET;
            break;
        case LogLevel::ERROR:
            prefix = RED + BOLD + "✖ " + RESET;
            break;
        case LogLevel::DEBUG:
            prefix = GRAY + "[Debug] " + RESET;
            break;
    }

    // Trim trailing newlines from message to avoid double spacing
    std::string trimmedMsg = message;
    while (!trimmedMsg.empty() && (trimmedMsg.back() == '\n' || trimmedMsg.back() == '\r')) {
        trimmedMsg.pop_back();
    }

    // Style Internal Monologue tags if present
    if (level == LogLevel::THOUGHT) {
        // Remove redundant legacy prefixes if they exist in the message content
        trimmedMsg = std::regex_replace(trimmedMsg, std::regex(R"(^🤔 \[Think\] )"), "");
        trimmedMsg = std::regex_replace(trimmedMsg, std::regex(R"(\n🤔 \[Think\] )"), "\n");
        
        trimmedMsg = std::regex_replace(trimmedMsg, std::regex(R"(\[THOUGHT\]:?)"), BOLD + CYAN + "💭 [Thought]" + RESET);
        trimmedMsg = std::regex_replace(trimmedMsg, std::regex(R"(\[PLAN\]:?)"), BOLD + MAGENTA + "📋 [Plan]" + RESET);
        trimmedMsg = std::regex_replace(trimmedMsg, std::regex(R"(\[REFLECTION\]:?)"), BOLD + YELLOW + "🧐 [Reflect]" + RESET);
    }

    // Handle multi-line messages by prepending prefix to each line
    std::stringstream ss(trimmedMsg);
    std::string line;
    bool first = true;
    while (std::getline(ss, line)) {
        if (level == LogLevel::THOUGHT) {
            if (first) {
                std::cout << prefix << line << std::endl;
            } else {
                // For subsequent thought lines, use a simpler indentation or vertical line
                std::cout << GRAY << "  │ " << RESET << line << std::endl;
            }
        } else {
            if (!first) {
                std::cout << prefix << line << std::endl;
            } else {
                std::cout << prefix << line << std::endl;
            }
        }
        first = false;
    }
}
