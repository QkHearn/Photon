/**
 * @file semantic_search_example.cpp
 * @brief 演示语义搜索作为 Agent 内部能力的使用
 * 
 * 这个示例展示了如何将 SemanticManager 集成到 AgentRuntime 中，
 * 实现自动化的代码搜索和定位功能。
 */

#include "agent/AgentRuntime.h"
#include "analysis/SemanticManager.h"
#include "analysis/SymbolManager.h"
#include "tools/ToolRegistry.h"
#include "tools/CoreTools.h"
#include "core/LLMClient.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== Photon 语义搜索演示 ===" << std::endl;
    
    // 1. 初始化项目路径
    std::string projectPath = "/path/to/your/project";
    
    // 2. 创建 LLM 客户端
    auto llmClient = std::make_shared<LLMClient>();
    // 配置 API 密钥、模型等...
    
    // 3. 创建符号管理器
    SymbolManager symbolManager(projectPath);
    symbolManager.startAsyncScan();
    
    // 4. 创建语义管理器
    auto semanticManager = std::make_shared<SemanticManager>(projectPath, llmClient);
    
    std::cout << "正在后台建立语义索引..." << std::endl;
    semanticManager->startAsyncIndexing();
    
    // 等待索引完成（生产环境中可以异步进行）
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 5. 创建工具注册表
    ToolRegistry toolRegistry;
    toolRegistry.registerTool(std::make_unique<ReadCodeBlockTool>(projectPath));
    toolRegistry.registerTool(std::make_unique<ListProjectFilesTool>(projectPath));
    
    // 6. 创建 AgentRuntime（传入 SemanticManager）
    AgentRuntime agent(
        llmClient,
        toolRegistry,
        &symbolManager,
        nullptr,              // MemoryManager
        nullptr,              // SkillManager
        semanticManager.get() // ← 关键：传入 SemanticManager
    );
    
    std::cout << "\n✅ Agent 初始化完成，语义搜索能力已启用\n" << std::endl;
    
    // ========================================
    // 场景 1: 用户模糊查询
    // ========================================
    std::cout << "【场景 1】用户模糊查询" << std::endl;
    std::cout << "用户: \"找到处理用户登录逻辑的代码\"" << std::endl;
    std::cout << std::endl;
    
    // Agent 执行任务（内部会自动触发语义搜索）
    agent.executeTask("找到处理用户登录逻辑的代码");
    
    // 预期流程：
    // 1. LLM 生成工具调用，可能是 read_code_block(path="登录逻辑")
    // 2. Agent 拦截，检测到模糊查询
    // 3. Agent 内部调用 semanticManager->search("登录逻辑")
    // 4. Agent 将搜索结果注入为系统消息
    // 5. LLM 看到搜索结果，选择正确的文件进行读取
    // 6. 返回给用户
    
    std::cout << "\n" << std::string(50, '=') << "\n" << std::endl;
    
    // ========================================
    // 场景 2: 直接使用语义搜索（测试）
    // ========================================
    std::cout << "【场景 2】直接调用语义搜索（测试用）" << std::endl;
    
    // 这里展示的是内部能力，实际使用中由 Agent 自动调用
    auto chunks = semanticManager->search("数据库连接错误处理", 3);
    
    std::cout << "搜索查询: \"数据库连接错误处理\"" << std::endl;
    std::cout << "找到 " << chunks.size() << " 个相关代码片段:\n" << std::endl;
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        std::cout << "[" << (i + 1) << "] " << chunk.path 
                  << " (lines " << chunk.startLine << "-" << chunk.endLine << ")" << std::endl;
        std::cout << "    相关度: " << (chunk.score * 100) << "%" << std::endl;
        std::cout << "    类型: " << chunk.type << std::endl;
        
        // 显示代码预览
        std::istringstream ss(chunk.content);
        std::string line;
        int lineCount = 0;
        while (std::getline(ss, line) && lineCount < 3) {
            std::cout << "    " << line << std::endl;
            lineCount++;
        }
        std::cout << "    ..." << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "\n" << std::string(50, '=') << "\n" << std::endl;
    
    // ========================================
    // 场景 3: 展示拦截工作原理
    // ========================================
    std::cout << "【场景 3】拦截工作原理演示" << std::endl;
    
    std::cout << "\n当 LLM 生成以下工具调用时：" << std::endl;
    std::cout << "  read_code_block(path=\"处理 API 请求的代码\")" << std::endl;
    std::cout << "\nAgent 会检测到：" << std::endl;
    std::cout << "  ✓ 路径包含空格" << std::endl;
    std::cout << "  ✓ 路径包含中文" << std::endl;
    std::cout << "  → 识别为语义查询" << std::endl;
    std::cout << "\nAgent 自动执行：" << std::endl;
    std::cout << "  1. semanticManager->search(\"处理 API 请求的代码\")" << std::endl;
    std::cout << "  2. 格式化搜索结果" << std::endl;
    std::cout << "  3. 注入为系统消息：" << std::endl;
    
    std::cout << "\n" << R"(
    🔎 [Agent Semantic Search] Found 3 relevant code locations:
    
    **[1] src/api/RequestHandler.cpp (lines 45-89)**
       Relevance: 92.35%
       Preview:
         class RequestHandler {
           void handleRequest(Request& req) {
         ...
    
    **[2] src/server/APIRouter.cpp (lines 120-150)**
       Relevance: 85.72%
       Preview:
         void APIRouter::route(Request& req, Response& res) {
         ...
    
    💡 Tip: Use read_code_block with the file paths above.
    )" << std::endl;
    
    std::cout << "\nLLM 收到这个消息后，可以：" << std::endl;
    std::cout << "  → 选择相关度最高的文件" << std::endl;
    std::cout << "  → 使用正确的路径和行号读取代码" << std::endl;
    std::cout << "  → 返回准确的结果给用户" << std::endl;
    
    std::cout << "\n" << std::string(50, '=') << "\n" << std::endl;
    
    // ========================================
    // 总结
    // ========================================
    std::cout << "【总结】语义搜索作为 Agent 内部能力的优势：\n" << std::endl;
    std::cout << "1. 🎯 智能增强：Agent 自动判断何时使用" << std::endl;
    std::cout << "2. 🔍 透明操作：LLM 无感知，只看到增强后的上下文" << std::endl;
    std::cout << "3. 🚀 高效定位：从模糊查询到精确代码" << std::endl;
    std::cout << "4. 📈 可扩展性：可以轻松添加更多内部能力" << std::endl;
    
    std::cout << "\n演示完成！" << std::endl;
    
    return 0;
}

/*
 * 编译说明：
 * 
 * cd build
 * cmake ..
 * make
 * 
 * 运行：
 * ./semantic_search_example
 * 
 * 注意事项：
 * 1. 需要配置有效的 LLM API 密钥
 * 2. 首次运行需要等待索引完成
 * 3. 索引会保存到 .photon/index/ 目录
 * 4. 可以通过 config.json 配置索引行为
 */
