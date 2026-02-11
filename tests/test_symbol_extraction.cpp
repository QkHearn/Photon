#include "src/analysis/SymbolManager.h"
#include "src/analysis/providers/TreeSitterSymbolProvider.h"
#include "src/analysis/providers/RegexSymbolProvider.h"
#include <iostream>
#include <fstream>

int main() {
    SymbolManager symbolManager(".");
    
    // 注册TreeSitter提供者
#ifdef PHOTON_ENABLE_TREESITTER
    auto treeProvider = std::make_unique<TreeSitterSymbolProvider>();
    treeProvider->registerLanguage("arkts", {".ets"}, tree_sitter_arkts());
    symbolManager.registerProvider(std::move(treeProvider));
    std::cout << "✓ TreeSitter provider registered for .ets files" << std::endl;
#endif
    
    // 注册Regex提供者
    auto regexProvider = std::make_unique<RegexSymbolProvider>();
    symbolManager.registerProvider(std::move(regexProvider));
    std::cout << "✓ Regex provider registered" << std::endl;
    
    // 扫描测试文件
    std::cout << "Scanning test_arkts.ets..." << std::endl;
    symbolManager.scanBlocking();
    
    // 获取符号
    auto symbols = symbolManager.getFileSymbols("test_arkts.ets");
    std::cout << "Found " << symbols.size() << " symbols:" << std::endl;
    
    for (const auto& symbol : symbols) {
        std::cout << "  - " << symbol.name << " (" << symbol.type << ") at line " 
                  << symbol.line << " [source: " << symbol.source << "]" << std::endl;
    }
    
    return 0;
}