#include "core/UIManager.h"
#include <iostream>

UIManager::UIManager() {
    setupLogger();
}

UIManager::~UIManager() {
    stop();
}

void UIManager::setupLogger() {
    Logger::getInstance().setCallback([this](LogLevel level, const std::string& msg) {
        // In CLI mode, the logger might already be printing to console, 
        // or we can handle specific levels here if needed.
    });
}

void UIManager::setMode(Mode mode) {
    // Only CLI mode supported now
}

void UIManager::start(InputCallback onInput) {
    if (running) return;
    this->inputCallback = onInput;
    running = true;
}

void UIManager::stop() {
    running = false;
}

void UIManager::addThought(const std::string& thought) {}
void UIManager::appendToLastThought(const std::string& delta) {}
void UIManager::addChatMessage(const std::string& role, const std::string& content) {}
void UIManager::appendToLastChat(const std::string& delta) {}
void UIManager::addAction(const std::string& action) {}
void UIManager::addSystemLog(const std::string& log, LogLevel level) {}
void UIManager::updateStatus(const std::string& model, int tokens, int tasks) {}
void UIManager::setDiff(const std::string& diff) {}
