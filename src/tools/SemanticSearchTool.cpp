#include "SemanticSearchTool.h"
#include "analysis/SemanticManager.h"
#include <sstream>
#include <iomanip>
#include <iostream>

SemanticSearchTool::SemanticSearchTool(SemanticManager* semanticMgr) 
    : semanticMgr(semanticMgr) {}

std::string SemanticSearchTool::getDescription() const {
    return "Search the codebase using natural language queries. "
           "This tool finds relevant code snippets based on semantic similarity, "
           "not just keyword matching. "
           "Use this when you need to find code by concept, functionality, or behavior. "
           "Parameters: query (string, required), top_k (int, optional, default 5).";
}

nlohmann::json SemanticSearchTool::getSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "Natural language query describing what you're looking for. "
                               "Examples: 'how is authentication handled?', 'where are files read?', "
                               "'code that processes user input'"}
            }},
            {"top_k", {
                {"type", "integer"},
                {"description", "Number of results to return (default: 5, max: 20)"},
                {"default", 5},
                {"minimum", 1},
                {"maximum", 20}
            }}
        }},
        {"required", {"query"}}
    };
}

nlohmann::json SemanticSearchTool::execute(const nlohmann::json& args) {
    nlohmann::json result;
    
    // 验证参数
    if (!args.contains("query")) {
        result["error"] = "Missing required parameter: query";
        return result;
    }
    
    std::string query = args["query"].get<std::string>();
    int topK = args.value("top_k", 5);
    
    // 限制范围
    if (topK < 1) topK = 1;
    if (topK > 20) topK = 20;
    
    if (!semanticMgr) {
        result["error"] = "SemanticManager not available";
        result["hint"] = "Semantic search requires embedding support. "
                        "Make sure the LLM client supports embeddings.";
        return result;
    }
    
    std::cout << "[SemanticSearch] Query: \"" << query << "\" (top_k=" << topK << ")" << std::endl;
    
    try {
        // 执行语义搜索
        auto chunks = semanticMgr->search(query, topK);
        
        if (chunks.empty()) {
            std::cout << "[SemanticSearch] No results found" << std::endl;
            result["error"] = "No relevant code found for this query";
            result["hint"] = "Try rephrasing your query or using more general terms";
            return result;
        }
        
        std::cout << "[SemanticSearch] Found " << chunks.size() << " results" << std::endl;
        
        // 格式化结果
        std::string formattedResults = formatSearchResults(chunks, query);
        
        nlohmann::json contentItem;
        contentItem["type"] = "text";
        contentItem["text"] = formattedResults;
        
        result["content"] = nlohmann::json::array({contentItem});
        result["result_count"] = static_cast<int>(chunks.size());
        
    } catch (const std::exception& e) {
        std::cerr << "[SemanticSearch] Error: " << e.what() << std::endl;
        result["error"] = std::string("Semantic search failed: ") + e.what();
    }
    
    return result;
}

std::string SemanticSearchTool::formatSearchResults(
    const std::vector<SemanticChunk>& chunks, 
    const std::string& query) {
    
    std::ostringstream output;
    
    output << "🔎 Semantic Search Results for: \"" << query << "\"\n\n";
    output << "Found " << chunks.size() << " relevant code locations:\n\n";
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        
        output << "**[" << (i + 1) << "] " << chunk.path;
        if (chunk.startLine > 0) {
            output << " (lines " << chunk.startLine << "-" << chunk.endLine << ")";
        }
        output << "**\n";
        
        output << "   Relevance: " << std::fixed << std::setprecision(1) 
               << (chunk.score * 100) << "%\n";
        
        if (!chunk.type.empty()) {
            output << "   Type: " << chunk.type << "\n";
        }
        
        // 显示代码片段预览
        output << "   Preview:\n";
        
        std::istringstream contentStream(chunk.content);
        std::string line;
        int lineCount = 0;
        int charCount = 0;
        const int maxLines = 4;
        const int maxChars = 200;
        
        while (std::getline(contentStream, line) && lineCount < maxLines && charCount < maxChars) {
            // 去除前后空白
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos) {
                line = line.substr(start);
            }
            
            if (!line.empty()) {
                output << "     " << line << "\n";
                charCount += line.length();
                lineCount++;
            }
        }
        
        if (lineCount >= maxLines || charCount >= maxChars) {
            output << "     ...\n";
        }
        
        output << "\n";
    }
    
    output << "💡 **Next Steps**:\n";
    output << "  - Use `read_code_block` with file path and line numbers to see full code\n";
    output << "  - Use `view_symbol` to see specific functions or classes\n";
    output << "  - Refine your query if results aren't relevant\n";
    
    return output.str();
}
