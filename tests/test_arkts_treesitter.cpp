#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#ifdef PHOTON_ENABLE_TREESITTER
#include "tree_sitter/api.h"
#include "tree_sitter/tree-sitter-arkts.h"
#endif

// 测试用的ArkTS代码
const char* arkts_test_code = R"(
@Component
struct MyComponent {
  @State message: string = 'Hello, ArkTS!'
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
      
      Button('Click Me')
        .onClick(() => {
          this.message = 'Button clicked!'
        })
    }
  }
}

function calculateSum(a: number, b: number): number {
  return a + b;
}

class Calculator {
  private value: number = 0;
  
  add(num: number): void {
    this.value += num;
  }
  
  getResult(): number {
    return this.value;
  }
}
)";

struct Symbol {
    std::string name;
    std::string type;
    int line;
    int column;
};

#ifdef PHOTON_ENABLE_TREESITTER
void collect_symbols(TSNode node, const std::string& code, std::vector<Symbol>& symbols, int level = 0) {
    const char* type = ts_node_type(node);
    
    // 打印节点类型用于调试
    for (int i = 0; i < level; i++) std::cout << "  ";
    std::cout << "Node: " << type << std::endl;
    
    // 提取不同类型的符号
    if (strcmp(type, "class_declaration") == 0 || 
        strcmp(type, "struct_declaration") == 0 ||
        strcmp(type, "interface_declaration") == 0) {
        
        // 查找类名
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "type_identifier") == 0) {
                TSPoint start = ts_node_start_point(child);
                uint32_t start_byte = ts_node_start_byte(child);
                uint32_t end_byte = ts_node_end_byte(child);
                
                if (end_byte > start_byte && end_byte <= code.size()) {
                    std::string name = code.substr(start_byte, end_byte - start_byte);
                    symbols.push_back({name, "class", static_cast<int>(start.row + 1), static_cast<int>(start.column + 1)});
                    std::cout << "  Found class: " << name << " at line " << (start.row + 1) << std::endl;
                }
                break;
            }
        }
    }
    else if (strcmp(type, "function_declaration") == 0 || 
             strcmp(type, "method_definition") == 0) {
        
        // 查找函数名
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "identifier") == 0) {
                TSPoint start = ts_node_start_point(child);
                uint32_t start_byte = ts_node_start_byte(child);
                uint32_t end_byte = ts_node_end_byte(child);
                
                if (end_byte > start_byte && end_byte <= code.size()) {
                    std::string name = code.substr(start_byte, end_byte - start_byte);
                    symbols.push_back({name, "function", static_cast<int>(start.row + 1), static_cast<int>(start.column + 1)});
                    std::cout << "  Found function: " << name << " at line " << (start.row + 1) << std::endl;
                }
                break;
            }
        }
    }
    else if (strcmp(type, "property_identifier") == 0) {
        // 可能的状态变量或其他属性
        TSPoint start = ts_node_start_point(node);
        uint32_t start_byte = ts_node_start_byte(node);
        uint32_t end_byte = ts_node_end_byte(node);
        
        if (end_byte > start_byte && end_byte <= code.size()) {
            std::string name = code.substr(start_byte, end_byte - start_byte);
            // 检查是否是状态变量（前面有@State）
            if (start.row > 0) {
                size_t line_start = code.rfind('\n', start_byte - 1);
                if (line_start != std::string::npos) {
                    std::string prev_line = code.substr(line_start + 1, start_byte - line_start - 1);
                    if (prev_line.find("@State") != std::string::npos) {
                        symbols.push_back({name, "state", static_cast<int>(start.row + 1), static_cast<int>(start.column + 1)});
                        std::cout << "  Found state: " << name << " at line " << (start.row + 1) << std::endl;
                    }
                }
            }
        }
    }
    
    // 递归处理子节点
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        collect_symbols(child, code, symbols, level + 1);
    }
}
#endif

int main() {
    std::cout << "=== ArkTS TreeSitter Test ===" << std::endl;
    
#ifndef PHOTON_ENABLE_TREESITTER
    std::cout << "❌ TreeSitter support is not enabled!" << std::endl;
    std::cout << "Please compile with -DPHOTON_ENABLE_TREESITTER flag" << std::endl;
    return 1;
#else
    std::cout << "✓ TreeSitter support is enabled" << std::endl;
    
    // 创建parser
    TSParser* parser = ts_parser_new();
    if (!parser) {
        std::cout << "❌ Failed to create parser" << std::endl;
        return 1;
    }
    std::cout << "✓ Parser created" << std::endl;
    
    // 设置ArkTS语言
    const TSLanguage* language = tree_sitter_arkts();
    if (!language) {
        std::cout << "❌ Failed to get ArkTS language" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    std::cout << "✓ ArkTS language obtained" << std::endl;
    
    bool lang_set = ts_parser_set_language(parser, language);
    if (!lang_set) {
        std::cout << "❌ Failed to set ArkTS language" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    std::cout << "✓ ArkTS language set successfully" << std::endl;
    
    // 解析代码
    std::string code = arkts_test_code;
    TSTree* tree = ts_parser_parse_string(parser, nullptr, code.c_str(), code.length());
    if (!tree) {
        std::cout << "❌ Failed to parse code" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    std::cout << "✓ Code parsed successfully" << std::endl;
    
    // 获取根节点
    TSNode root_node = ts_tree_root_node(tree);
    std::cout << "\n=== Parsing AST Structure ===" << std::endl;
    std::cout << "Root node type: " << ts_node_type(root_node) << std::endl;
    
    // 收集符号
    std::cout << "\n=== Extracted Symbols ===" << std::endl;
    std::vector<Symbol> symbols;
    collect_symbols(root_node, code, symbols);
    
    // 总结结果
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total symbols found: " << symbols.size() << std::endl;
    
    if (symbols.empty()) {
        std::cout << "⚠️  No symbols were extracted. This might indicate:" << std::endl;
        std::cout << "  1. The ArkTS grammar doesn't recognize the syntax patterns" << std::endl;
        std::cout << "  2. The tree structure is different than expected" << std::endl;
        std::cout << "  3. The test code needs to be adjusted" << std::endl;
    } else {
        std::cout << "✓ Symbols successfully extracted:" << std::endl;
        for (const auto& symbol : symbols) {
            std::cout << "  - " << symbol.name << " (" << symbol.type << ") at line " 
                      << symbol.line << ":" << symbol.column << std::endl;
        }
    }
    
    // 清理
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return symbols.empty() ? 1 : 0;
#endif
}