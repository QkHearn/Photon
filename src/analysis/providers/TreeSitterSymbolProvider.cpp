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
    
    auto makeSymbol = [&](const char* symbolType) {
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
        out.push_back({name, symbolType, "tree_sitter", relPath, 
                      static_cast<int>(start.row + 1), 
                      static_cast<int>(end.row + 1), ""});
    };

    if (std::strcmp(type, "class_specifier") == 0) {
        makeSymbol("class");
    } else if (std::strcmp(type, "struct_specifier") == 0) {
        makeSymbol("struct");
    } else if (std::strcmp(type, "function_definition") == 0 ||
               std::strcmp(type, "function_declaration") == 0 ||
               std::strcmp(type, "method_definition") == 0) {
        makeSymbol("function");
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
