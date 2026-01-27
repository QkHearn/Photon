#include "utils/TreeSitterSymbolProvider.h"
#include <cstring>
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
    if (!lang || !lang->language) return results;

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

    auto makeSymbol = [&](const char* symbolType) {
        uint32_t childCount = ts_node_child_count(node);
        for (uint32_t i = 0; i < childCount; ++i) {
            TSNode child = ts_node_child(node, i);
            const char* childType = ts_node_type(child);
            if (std::strcmp(childType, "identifier") == 0 || std::strcmp(childType, "name") == 0) {
                auto start = ts_node_start_point(child);
                auto startByte = ts_node_start_byte(child);
                auto endByte = ts_node_end_byte(child);
                std::string name;
                if (endByte > startByte && endByte <= content.size()) {
                    name = content.substr(startByte, endByte - startByte);
                } else {
                    name = childType;
                }
                out.push_back({name, symbolType, "tree_sitter", relPath, static_cast<int>(start.row + 1), ""});
                break;
            }
        }
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
