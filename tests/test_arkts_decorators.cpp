#include <iostream>
#include <fstream>
#include <filesystem>
#include "src/analysis/SymbolManager.h"
#include "src/analysis/providers/TreeSitterSymbolProvider.h"

namespace fs = std::filesystem;

int main() {
    std::cout << "=== ArkTS Decorators Test ===" << std::endl;
    
    // 创建测试目录结构
    fs::path test_dir = "decorators_test_dir";
    fs::create_directories(test_dir);
    
    // 创建包含各种装饰器的ArkTS测试文件
    fs::path test_file = test_dir / "decorators_test.ets";
    std::ofstream file(test_file);
    file << R"(// ArkTS 装饰器测试文件

@Component
struct MyComponent {
  @State private message: string = 'Hello World'
  @State count: number = 0
  @Prop title: string = 'Default Title'
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(20)
      
      Button('Click Me')
        .onClick(() => {
          this.count++
        })
    }
  }
  
  private helperMethod(): void {
    console.log('Helper called')
  }
}

@Entry
@Component
struct MainEntry {
  @State isVisible: boolean = true
  
  build() {
    Row() {
      if (this.isVisible) {
        MyComponent({ title: 'Test' })
      }
    }
  }
}

// 普通类和函数
class RegularClass {
  private value: number = 42
  
  getValue(): number {
    return this.value
  }
}

function utilityFunction(): string {
  return 'utility'
}

// 带装饰器的函数
@Deprecated
function oldFunction(): void {
  console.log('This is deprecated')
}

// 状态管理装饰器示例
@Observed
class AppState {
  @Track userName: string = ''
  @Track isLoggedIn: boolean = false
}
)";
    file.close();
    
    std::cout << "✓ Created test ArkTS file with decorators: " << test_file << std::endl;
    
    // 初始化SymbolManager
    SymbolManager symbolManager(test_dir.string());
    
#ifdef PHOTON_ENABLE_TREESITTER
    // 注册TreeSitter提供者
    auto treeProvider = std::make_unique<TreeSitterSymbolProvider>();
    treeProvider->registerLanguage("arkts", {".ets"}, tree_sitter_arkts());
    symbolManager.registerProvider(std::move(treeProvider));
    std::cout << "✓ TreeSitter provider registered for .ets files" << std::endl;
#endif
    
    // 执行符号扫描
    std::cout << "\n=== Scanning ArkTS file with decorators ===" << std::endl;
    symbolManager.scanBlocking();
    
    // 获取文件符号
    auto symbols = symbolManager.getFileSymbols("decorators_test.ets");
    std::cout << "\n=== Decorator Extraction Results ===" << std::endl;
    std::cout << "Total symbols found: " << symbols.size() << std::endl;
    
    if (symbols.empty()) {
        std::cout << "⚠️  No symbols extracted from ArkTS file with decorators" << std::endl;
    } else {
        std::cout << "✓ Symbols successfully extracted:" << std::endl;
        
        // 按类型分组统计
        std::map<std::string, std::vector<SymbolManager::Symbol>> symbols_by_type;
        std::map<std::string, std::vector<SymbolManager::Symbol>> decorated_symbols;
        
        for (const auto& symbol : symbols) {
            symbols_by_type[symbol.type].push_back(symbol);
            
            // 检查是否有装饰器信息
            if (symbol.type.find("decorated:") != std::string::npos) {
                decorated_symbols[symbol.type].push_back(symbol);
            }
        }
        
        std::cout << "\n--- By Type ---" << std::endl;
        for (const auto& [type, type_symbols] : symbols_by_type) {
            std::cout << type << ": " << type_symbols.size() << " symbols" << std::endl;
            for (const auto& sym : type_symbols) {
                std::cout << "  - " << sym.name << " (line " << sym.line << ")";
                if (!sym.signature.empty()) {
                    std::cout << " - " << sym.signature;
                }
                std::cout << std::endl;
            }
        }
        
        // 专门的装饰器分析
        std::cout << "\n--- Decorator Analysis ---" << std::endl;
        
        // 统计不同类型的装饰器
        std::map<std::string, int> decorator_counts;
        std::vector<std::string> component_symbols;
        std::vector<std::string> state_symbols;
        std::vector<std::string> decorator_symbols;
        
        for (const auto& symbol : symbols) {
            if (symbol.type.find("decorated:") != std::string::npos) {
                // 提取装饰器信息
                size_t pos = symbol.type.find("decorated:");
                std::string decorators = symbol.type.substr(pos + 10); // "decorated:" 长度是10
                
                // 分割多个装饰器
                size_t comma_pos = 0;
                std::string token;
                std::string delimiter = ",";
                std::string s = decorators;
                while ((comma_pos = s.find(delimiter)) != std::string::npos) {
                    token = s.substr(0, comma_pos);
                    decorator_counts[token]++;
                    s.erase(0, comma_pos + delimiter.length());
                }
                if (!s.empty()) {
                    decorator_counts[s]++;
                }
                
                // 分类符号
                if (decorators.find("Component") != std::string::npos) {
                    component_symbols.push_back(symbol.name);
                }
                if (decorators.find("State") != std::string::npos) {
                    state_symbols.push_back(symbol.name);
                }
                if (symbol.type == "decorator") {
                    decorator_symbols.push_back(symbol.name);
                }
            }
            
            // 检查特定的ArkTS类型
            if (symbol.type == "component") {
                component_symbols.push_back(symbol.name);
            }
            if (symbol.type == "state") {
                state_symbols.push_back(symbol.name);
            }
            if (symbol.type == "build_method") {
                std::cout << "✓ Found build method: " << symbol.name << std::endl;
            }
        }
        
        std::cout << "\n--- Decorator Summary ---" << std::endl;
        std::cout << "Decorators found:" << std::endl;
        for (const auto& [decorator, count] : decorator_counts) {
            std::cout << "  @" << decorator << ": " << count << " occurrences" << std::endl;
        }
        
        std::cout << "\nComponents with @Component: " << component_symbols.size() << std::endl;
        for (const auto& name : component_symbols) {
            std::cout << "  - " << name << std::endl;
        }
        
        std::cout << "\nState variables with @State: " << state_symbols.size() << std::endl;
        for (const auto& name : state_symbols) {
            std::cout << "  - " << name << std::endl;
        }
        
        std::cout << "\nDecorator symbols: " << decorator_symbols.size() << std::endl;
        for (const auto& name : decorator_symbols) {
            std::cout << "  - @" << name << std::endl;
        }
        
        // 验证关键装饰器
        std::cout << "\n--- Key Decorator Verification ---" << std::endl;
        bool has_component = !component_symbols.empty();
        bool has_state = !state_symbols.empty();
        bool has_build_method = false;
        
        for (const auto& symbol : symbols) {
            if (symbol.type == "build_method") {
                has_build_method = true;
                break;
            }
        }
        
        std::cout << "@Component detection: " << (has_component ? "✅ PASS" : "❌ FAIL") << std::endl;
        std::cout << "@State detection: " << (has_state ? "✅ PASS" : "❌ FAIL") << std::endl;
        std::cout << "build() method detection: " << (has_build_method ? "✅ PASS" : "❌ FAIL") << std::endl;
        
        // 总体评估
        bool overall_success = has_component && has_state;
        std::cout << "\n--- Overall Assessment ---" << std::endl;
        if (overall_success) {
            std::cout << "🎉 ArkTS decorator extraction is WORKING correctly!" << std::endl;
        } else {
            std::cout << "⚠️  Some decorator features may need refinement" << std::endl;
        }
    }
    
    // 清理测试文件
    fs::remove_all(test_dir);
    std::cout << "\n✓ Test cleanup completed" << std::endl;
    
    std::cout << "\n=== Decorator Test Complete ===" << std::endl;
    return 0;
}