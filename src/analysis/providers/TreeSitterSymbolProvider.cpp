#include "analysis/providers/TreeSitterSymbolProvider.h"
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <functional>
#ifdef PHOTON_ENABLE_TREESITTER
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

TreeSitterSymbolProvider::TreeSitterSymbolProvider() = default;

TreeSitterSymbolProvider::~TreeSitterSymbolProvider() {
#ifdef PHOTON_ENABLE_TREESITTER
#ifdef _WIN32
    for (auto* handle : handles) {
        if (handle) {
            FreeLibrary(reinterpret_cast<HMODULE>(handle));
        }
    }
#else
    for (auto* handle : handles) {
        if (handle) {
            dlclose(handle);
        }
    }
#endif
#endif
}

std::vector<Symbol> TreeSitterSymbolProvider::extractSymbols(const std::string& content, const std::string& relPath) const {
    (void)content;
    (void)relPath;
#ifdef PHOTON_ENABLE_TREESITTER
    std::vector<Symbol> results;
    const Language* lang = nullptr;
    for (const auto& entry : languages) {
        for (const auto& ext : entry.extensions) {
            if (relPath.size() >= ext.size() &&
                relPath.compare(relPath.size() - ext.size(), ext.size(), ext) == 0) {
                lang = &entry;
                break;
            }
        }
        if (lang) break;
    }
    if (!lang || !lang->language) {
        return results;
    }

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, lang->language);
    TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.size());
    TSNode root = ts_tree_root_node(tree);

    collectSymbols(root, relPath, content, results);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return results;
#else
    return {};
#endif
}

bool TreeSitterSymbolProvider::supportsExtension(const std::string& ext) const {
    for (const auto& entry : languages) {
        for (const auto& e : entry.extensions) {
            if (e == ext) return true;
        }
    }
    return false;
}

#ifdef PHOTON_ENABLE_TREESITTER
void TreeSitterSymbolProvider::registerLanguage(const std::string& name,
                                                const std::vector<std::string>& extensions,
                                                const TSLanguage* language) {
    if (!language || extensions.empty()) return;
    languages.push_back({name, extensions, language});
}

bool TreeSitterSymbolProvider::registerLanguageFromLibrary(const std::string& name,
                                                           const std::vector<std::string>& extensions,
                                                           const std::string& libraryPath,
                                                           const std::string& symbolName) {
    if (libraryPath.empty() || symbolName.empty()) return false;
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(libraryPath.c_str());
    if (!handle) return false;
    auto symbol = reinterpret_cast<const TSLanguage* (*)()>(
        GetProcAddress(handle, symbolName.c_str()));
    if (!symbol) {
        FreeLibrary(handle);
        return false;
    }
    registerLanguage(name, extensions, symbol());
    handles.push_back(reinterpret_cast<void*>(handle));
    return true;
#else
    void* handle = dlopen(libraryPath.c_str(), RTLD_NOW);
    if (!handle) return false;
    auto symbol = reinterpret_cast<const TSLanguage* (*)()>(
        dlsym(handle, symbolName.c_str()));
    if (!symbol) {
        dlclose(handle);
        return false;
    }
    registerLanguage(name, extensions, symbol());
    handles.push_back(handle);
    return true;
#endif
}

const TreeSitterSymbolProvider::Language* TreeSitterSymbolProvider::languageForExtension(const std::string& ext) const {
    for (const auto& entry : languages) {
        for (const auto& e : entry.extensions) {
            if (e == ext) return &entry;
        }
    }
    return nullptr;
}

void TreeSitterSymbolProvider::collectSymbols(TSNode node,
                                              const std::string& relPath,
                                              const std::string& content,
                                              std::vector<Symbol>& out) const {
    const char* type = ts_node_type(node);

    auto findIdentifier = [&](TSNode parent) -> TSNode {
        // 递归查找第一个 identifier 节点
        std::function<TSNode(TSNode)> search = [&](TSNode n) -> TSNode {
            const char* nodeType = ts_node_type(n);
            if (std::strcmp(nodeType, "identifier") == 0 || 
                std::strcmp(nodeType, "name") == 0 || 
                std::strcmp(nodeType, "field_identifier") == 0 ||
                std::strcmp(nodeType, "type_identifier") == 0) {
                return n;
            }
            uint32_t count = ts_node_child_count(n);
            for (uint32_t i = 0; i < count; ++i) {
                TSNode result = search(ts_node_child(n, i));
                if (!ts_node_is_null(result)) {
                    return result;
                }
            }
            TSNode nullNode = {};
            return nullNode;
        };
        return search(parent);
    };
    
    auto getDecoratorName = [&](TSNode node) -> std::string {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            const char* childType = ts_node_type(child);
            if (std::strcmp(childType, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (end > start && end <= content.size()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "";
    };
    
    auto getDecoratorArguments = [&](TSNode node) -> std::string {
        // 查找装饰器的参数（如果有）
        uint32_t count = ts_node_child_count(node);
        std::string args;
        
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            const char* childType = ts_node_type(child);
            
            // 查找参数列表
            if (std::strcmp(childType, "arguments") == 0 || 
                std::strcmp(childType, "decorator_arguments") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (end > start && end <= content.size()) {
                    args = content.substr(start, end - start);
                    break;
                }
            }
            
            // 查找字符串字面量参数（如@BuilderParam("headerBuilder")）
            if (std::strcmp(childType, "string_literal") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (end > start && end <= content.size()) {
                    if (!args.empty()) args += ", ";
                    args += content.substr(start, end - start);
                }
            }
            
            // 查找标识符参数（如@LocalStorageProp('theme')）
            if (std::strcmp(childType, "identifier") == 0 && i > 0) {
                // 确保这不是装饰器名称本身
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (end > start && end <= content.size()) {
                    std::string argName = content.substr(start, end - start);
                    if (argName != getDecoratorName(node)) {
                        if (!args.empty()) args += ", ";
                        args += argName;
                    }
                }
            }
        }
        
        return args;
    };
    
    // ArkTS装饰器分类
    auto classifyDecorator = [&](const std::string& decoratorName) -> std::string {
        // 组件与入口装饰器
        if (decoratorName == "Component" || decoratorName == "ComponentV2" || 
            decoratorName == "Entry" || decoratorName == "Reusable") {
            return "component";
        }
        
        // 状态管理装饰器
        if (decoratorName == "State" || decoratorName == "Prop" || 
            decoratorName == "Link" || decoratorName == "ObjectLink" ||
            decoratorName == "Provide" || decoratorName == "Consume" ||
            decoratorName == "StorageLink" || decoratorName == "StorageProp" ||
            decoratorName == "LocalStorageLink" || decoratorName == "LocalStorageProp" ||
            decoratorName == "Local" || decoratorName == "Param" ||
            decoratorName == "Once" || decoratorName == "Event") {
            return "state_management";
        }
        
        // 观察者装饰器
        if (decoratorName == "Observed" || decoratorName == "ObservedV2" ||
            decoratorName == "Track" || decoratorName == "Watch" ||
            decoratorName == "Monitor") {
            return "observation";
        }
        
        // UI构建装饰器
        if (decoratorName == "Builder" || decoratorName == "BuilderParam" ||
            decoratorName == "LocalBuilder") {
            return "ui_builder";
        }
        
        // 样式扩展装饰器
        if (decoratorName == "Extend" || decoratorName == "AnimatableExtend" ||
            decoratorName == "Styles") {
            return "styling";
        }
        
        // 其他装饰器
        if (decoratorName == "Require" || decoratorName == "Type" ||
            decoratorName == "Trace" || decoratorName == "Computed") {
            return "other";
        }
        
        // 通用装饰器（TypeScript兼容）
        if (decoratorName == "Deprecated") {
            return "typescript";
        }
        
        return "unknown";
    };
    
    auto makeSymbol = [&](const char* symbolType, const std::string& decoratorInfo = "", const std::string& decoratorArgs = "") {
        TSNode identNode = findIdentifier(node);
        if (ts_node_is_null(identNode)) {
            return;
        }
        
        auto start = ts_node_start_point(node);
        auto end = ts_node_end_point(node);
        auto startByte = ts_node_start_byte(identNode);
        auto endByte = ts_node_end_byte(identNode);
        std::string name;
        if (endByte > startByte && endByte <= content.size()) {
            name = content.substr(startByte, endByte - startByte);
        } else {
            name = ts_node_type(identNode);
        }
        
        std::string finalType = symbolType;
        std::string finalSignature = "";
        
        if (!decoratorInfo.empty()) {
            finalType = finalType + ":" + decoratorInfo;
        }
        
        if (!decoratorArgs.empty()) {
            finalSignature = decoratorArgs;
        }
        
        out.push_back({name, finalType, "tree_sitter", relPath, 
                      static_cast<int>(start.row + 1), 
                      static_cast<int>(end.row + 1), finalSignature});
    };
    
    auto hasDecorator = [&](TSNode node, const std::string& decoratorName) -> bool {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (std::strcmp(ts_node_type(child), "decorator") == 0) {
                std::string name = getDecoratorName(child);
                if (name == decoratorName) {
                    return true;
                }
            }
        }
        return false;
    };
    
    auto hasStateDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "State");
    };
    
    auto hasPropDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "Prop");
    };
    
    auto hasLinkDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "Link") || hasDecorator(node, "ObjectLink");
    };
    
    auto hasObservedDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "Observed") || hasDecorator(node, "ObservedV2");
    };
    
    auto hasBuilderDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "Builder") || hasDecorator(node, "BuilderParam") || 
               hasDecorator(node, "LocalBuilder");
    };
    
    auto hasExtendDecorator = [&](TSNode node) -> bool {
        return hasDecorator(node, "Extend") || hasDecorator(node, "AnimatableExtend") ||
               hasDecorator(node, "Styles");
    };
    
    auto collectDecorators = [&](TSNode node) -> std::vector<std::string> {
        std::vector<std::string> decorators;
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (std::strcmp(ts_node_type(child), "decorator") == 0) {
                std::string name = getDecoratorName(child);
                if (!name.empty()) {
                    decorators.push_back(name);
                }
            }
        }
        return decorators;
    };

    // ArkTS 特定处理 - 按优先级排序
    if (std::strcmp(type, "decorator") == 0) {
        // 单独的装饰器节点，记录装饰器本身
        std::string decoratorName = getDecoratorName(node);
        if (!decoratorName.empty()) {
            auto start = ts_node_start_point(node);
            auto end = ts_node_end_point(node);
            std::string decoratorType = classifyDecorator(decoratorName);
            std::string args = getDecoratorArguments(node);
            
            std::string signature = args.empty() ? "" : "(" + args + ")";
            out.push_back({decoratorName, "decorator:" + decoratorType, "tree_sitter", relPath,
                          static_cast<int>(start.row + 1),
                          static_cast<int>(end.row + 1), signature});
        }
    } else if (std::strcmp(type, "component_declaration") == 0) {
        auto decorators = collectDecorators(node);
        std::string decoratorInfo;
        std::string decoratorArgs;
        
        if (!decorators.empty()) {
            decoratorInfo = "decorated:";
            for (size_t i = 0; i < decorators.size(); ++i) {
                if (i > 0) decoratorInfo += ",";
                decoratorInfo += decorators[i];
                
                // 获取每个装饰器的参数
                uint32_t count = ts_node_child_count(node);
                for (uint32_t j = 0; j < count; ++j) {
                    TSNode child = ts_node_child(node, j);
                    if (std::strcmp(ts_node_type(child), "decorator") == 0) {
                        std::string name = getDecoratorName(child);
                        if (name == decorators[i]) {
                            std::string args = getDecoratorArguments(child);
                            if (!args.empty()) {
                                if (!decoratorArgs.empty()) decoratorArgs += "; ";
                                decoratorArgs += name + "(" + args + ")";
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // 检查是否有@Entry装饰器
        if (hasDecorator(node, "Entry")) {
            makeSymbol("entry_component", decoratorInfo, decoratorArgs);
        } else if (hasDecorator(node, "Component") || hasDecorator(node, "ComponentV2")) {
            makeSymbol("component", decoratorInfo, decoratorArgs);
        } else {
            makeSymbol("component", decoratorInfo, decoratorArgs);
        }
    } else if (std::strcmp(type, "property_declaration") == 0) {
        auto decorators = collectDecorators(node);
        std::string decoratorInfo;
        std::string decoratorArgs;
        
        if (!decorators.empty()) {
            decoratorInfo = "decorated:";
            for (size_t i = 0; i < decorators.size(); ++i) {
                if (i > 0) decoratorInfo += ",";
                decoratorInfo += decorators[i];
                
                // 获取每个装饰器的参数
                uint32_t count = ts_node_child_count(node);
                for (uint32_t j = 0; j < count; ++j) {
                    TSNode child = ts_node_child(node, j);
                    if (std::strcmp(ts_node_type(child), "decorator") == 0) {
                        std::string name = getDecoratorName(child);
                        if (name == decorators[i]) {
                            std::string args = getDecoratorArguments(child);
                            if (!args.empty()) {
                                if (!decoratorArgs.empty()) decoratorArgs += "; ";
                                decoratorArgs += name + "(" + args + ")";
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        // 精确的状态管理装饰器识别
        if (hasStateDecorator(node)) {
            makeSymbol("state_variable", decoratorInfo, decoratorArgs);
        } else if (hasPropDecorator(node)) {
            makeSymbol("prop_variable", decoratorInfo, decoratorArgs);
        } else if (hasLinkDecorator(node)) {
            makeSymbol("link_variable", decoratorInfo, decoratorArgs);
        } else if (hasObservedDecorator(node)) {
            makeSymbol("observed_variable", decoratorInfo, decoratorArgs);
        } else {
            makeSymbol("property", decoratorInfo, decoratorArgs);
        }
    } else if (std::strcmp(type, "function_declaration") == 0 || 
               std::strcmp(type, "method_declaration") == 0) {
        // 检查函数是否有装饰器
        if (hasBuilderDecorator(node)) {
            makeSymbol("builder_function");
        } else if (hasExtendDecorator(node)) {
            makeSymbol("extend_function");
        } else {
            makeSymbol("function");
        }
    } else if (std::strcmp(type, "class_declaration") == 0) {
        // 检查类是否有装饰器
        if (hasObservedDecorator(node)) {
            makeSymbol("observed_class");
        } else {
            makeSymbol("class");
        }
    } else if (std::strcmp(type, "build_method") == 0) {
        makeSymbol("build_method");
    } else if (std::strcmp(type, "class_specifier") == 0) {
        makeSymbol("class");
    } else if (std::strcmp(type, "struct_specifier") == 0) {
        makeSymbol("struct");
    } else if (std::strcmp(type, "function_definition") == 0 ||
               std::strcmp(type, "function_declaration") == 0 ||
               std::strcmp(type, "method_definition") == 0) {
        makeSymbol("function");
    } else if (std::strcmp(type, "interface_declaration") == 0) {
        makeSymbol("interface");
    } else if (std::strcmp(type, "enum_declaration") == 0) {
        makeSymbol("enum");
    }

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        collectSymbols(ts_node_child(node, i), relPath, content, out);
    }
}
#endif

std::vector<TreeSitterSymbolProvider::CallInfo> TreeSitterSymbolProvider::extractCalls(const std::string& content, const std::string& relPath, int startLine, int endLine) const {
    std::vector<CallInfo> calls;
#ifdef PHOTON_ENABLE_TREESITTER
    const Language* lang = languageForExtension(fs::path(relPath).extension().u8string());
    if (!lang || !lang->language) return calls;

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, lang->language);
    TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.size());
    TSNode root = ts_tree_root_node(tree);

    std::vector<TSNode> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        TSNode node = stack.back();
        stack.pop_back();

        auto start = ts_node_start_point(node);
        auto end = ts_node_end_point(node);

        // If node is outside the range, skip it
        if (static_cast<int>(end.row + 1) < startLine || static_cast<int>(start.row + 1) > endLine) {
            continue;
        }

        const char* type = ts_node_type(node);
        if (std::strcmp(type, "call_expression") == 0) {
            // Find the function name/identifier
            TSNode funcNode = ts_node_child_by_field_name(node, "function", 8);
            if (ts_node_is_null(funcNode)) {
                uint32_t childCount = ts_node_child_count(node);
                for (uint32_t i = 0; i < childCount; ++i) {
                    TSNode child = ts_node_child(node, i);
                    if (std::strcmp(ts_node_type(child), "identifier") == 0) {
                        funcNode = child;
                        break;
                    }
                }
            }

            if (!ts_node_is_null(funcNode)) {
                if (std::strcmp(ts_node_type(funcNode), "field_expression") == 0) {
                    funcNode = ts_node_child_by_field_name(funcNode, "field", 5);
                }

                auto fStart = ts_node_start_point(funcNode);
                auto fStartByte = ts_node_start_byte(funcNode);
                auto fEndByte = ts_node_end_byte(funcNode);

                if (fEndByte > fStartByte && fEndByte <= content.size()) {
                    std::string name = content.substr(fStartByte, fEndByte - fStartByte);
                    calls.push_back({name, static_cast<int>(fStart.row + 1), static_cast<int>(fStart.column)});
                }
            }
        }

        uint32_t childCount = ts_node_child_count(node);
        for (int32_t i = static_cast<int32_t>(childCount) - 1; i >= 0; --i) {
            stack.push_back(ts_node_child(node, i));
        }
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
#endif
    return calls;
}
