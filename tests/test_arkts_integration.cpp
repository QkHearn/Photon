#include <iostream>
#include <fstream>
#include <filesystem>
#include "src/analysis/SymbolManager.h"
#include "src/analysis/providers/TreeSitterSymbolProvider.h"
#include "src/analysis/providers/RegexSymbolProvider.h"

namespace fs = std::filesystem;

int main() {
    std::cout << "=== ArkTS Integration Test ===" << std::endl;
    
    // 创建测试目录结构
    fs::path test_dir = "arkts_test_dir";
    fs::create_directories(test_dir);
    
    // 创建测试用的ArkTS文件
    fs::path test_file = test_dir / "test_app.ets";
    std::ofstream file(test_file);
    file << R"(@Component
struct TodoApp {
  @State todoItems: string[] = ['Learn ArkTS', 'Build App', 'Test Features']
  @State newItem: string = ''
  
  build() {
    Column() {
      Text('Todo List')
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
      
      ForEach(this.todoItems, (item: string) => {
        Row() {
          Text(item)
            .fontSize(16)
          Button('Delete')
            .onClick(() => {
              this.deleteItem(item)
            })
        }
      })
      
      Row() {
        TextInput({ placeholder: 'Add new item' })
          .onChange((value: string) => {
            this.newItem = value
          })
        Button('Add')
          .onClick(() => {
            this.addItem()
          })
      }
    }
  }
  
  addItem(): void {
    if (this.newItem.trim() !== '') {
      this.todoItems.push(this.newItem.trim())
      this.newItem = ''
    }
  }
  
  deleteItem(item: string): void {
    const index = this.todoItems.indexOf(item)
    if (index > -1) {
      this.todoItems.splice(index, 1)
    }
  }
}

// 工具函数
function formatDate(date: Date): string {
  return date.toLocaleDateString()
}

class TodoService {
  private todos: string[] = []
  
  getAllTodos(): string[] {
    return this.todos
  }
  
  addTodo(todo: string): void {
    this.todos.push(todo)
  }
  
  removeTodo(todo: string): boolean {
    const index = this.todos.indexOf(todo)
    if (index > -1) {
      this.todos.splice(index, 1)
      return true
    }
    return false
  }
})";
    file.close();
    
    std::cout << "✓ Created test ArkTS file: " << test_file << std::endl;
    
    // 初始化SymbolManager
    SymbolManager symbolManager(test_dir.string());
    
#ifdef PHOTON_ENABLE_TREESITTER
    // 注册TreeSitter提供者
    auto treeProvider = std::make_unique<TreeSitterSymbolProvider>();
    treeProvider->registerLanguage("arkts", {".ets"}, tree_sitter_arkts());
    symbolManager.registerProvider(std::move(treeProvider));
    std::cout << "✓ TreeSitter provider registered for .ets files" << std::endl;
#endif
    
    // 注册Regex提供者
    auto regexProvider = std::make_unique<RegexSymbolProvider>();
    symbolManager.registerProvider(std::move(regexProvider));
    std::cout << "✓ Regex provider registered" << std::endl;
    
    // 执行符号扫描
    std::cout << "\n=== Scanning ArkTS file ===" << std::endl;
    symbolManager.scanBlocking();
    
    // 获取文件符号
    auto symbols = symbolManager.getFileSymbols("test_app.ets");
    std::cout << "\n=== Symbol Extraction Results ===" << std::endl;
    std::cout << "Total symbols found: " << symbols.size() << std::endl;
    
    if (symbols.empty()) {
        std::cout << "⚠️  No symbols extracted from ArkTS file" << std::endl;
    } else {
        std::cout << "✓ Symbols successfully extracted:" << std::endl;
        
        // 按类型分组统计
        std::map<std::string, std::vector<SymbolManager::Symbol>> symbols_by_type;
        std::map<std::string, int> source_count;
        
        for (const auto& symbol : symbols) {
            symbols_by_type[symbol.type].push_back(symbol);
            source_count[symbol.source]++;
        }
        
        std::cout << "\n--- By Type ---" << std::endl;
        for (const auto& [type, type_symbols] : symbols_by_type) {
            std::cout << type << ": " << type_symbols.size() << " symbols" << std::endl;
            for (const auto& sym : type_symbols) {
                std::cout << "  - " << sym.name << " (line " << sym.line << ")" << std::endl;
            }
        }
        
        std::cout << "\n--- By Source ---" << std::endl;
        for (const auto& [source, count] : source_count) {
            std::cout << source << ": " << count << " symbols" << std::endl;
        }
        
        // 验证关键ArkTS特性
        std::cout << "\n=== ArkTS Feature Verification ===" << std::endl;
        
        bool has_component = false;
        bool has_state = false;
        bool has_build_method = false;
        bool has_ui_elements = false;
        
        for (const auto& symbol : symbols) {
            if (symbol.name == "TodoApp" && symbol.type == "class") {
                has_component = true;
            }
            if (symbol.type == "state" || symbol.name.find("Items") != std::string::npos) {
                has_state = true;
            }
            if (symbol.name == "build" && symbol.type == "function") {
                has_build_method = true;
            }
            if (symbol.name == "addItem" || symbol.name == "deleteItem") {
                has_ui_elements = true;
            }
        }
        
        std::cout << "Component structure: " << (has_component ? "✓" : "✗") << std::endl;
        std::cout << "State variables: " << (has_state ? "✓" : "✗") << std::endl;
        std::cout << "Build method: " << (has_build_method ? "✓" : "✗") << std::endl;
        std::cout << "UI event handlers: " << (has_ui_elements ? "✓" : "✗") << std::endl;
        
        // 搜索功能测试
        std::cout << "\n=== Search Functionality Test ===" << std::endl;
        auto search_results = symbolManager.search("Todo");
        std::cout << "Search for 'Todo': " << search_results.size() << " results" << std::endl;
        for (const auto& result : search_results) {
            std::cout << "  - " << result.name << " in " << result.path << std::endl;
        }
    }
    
    // 清理测试文件
    fs::remove_all(test_dir);
    std::cout << "\n✓ Test cleanup completed" << std::endl;
    
    std::cout << "\n=== Integration Test Complete ===" << std::endl;
    return symbols.empty() ? 1 : 0;
}