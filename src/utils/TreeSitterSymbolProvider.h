#pragma once
#include "utils/SymbolManager.h"
#include <string>
#include <vector>

#ifdef PHOTON_ENABLE_TREESITTER
#include <tree_sitter/api.h>
#endif

class TreeSitterSymbolProvider : public ISymbolProvider {
public:
    TreeSitterSymbolProvider();
    ~TreeSitterSymbolProvider();
    std::vector<Symbol> extractSymbols(const std::string& content, const std::string& relPath) const override;
    bool supportsExtension(const std::string& ext) const override;

    struct CallInfo {
        std::string name;
        int line;
        int character;
    };
    std::vector<CallInfo> extractCalls(const std::string& content, const std::string& relPath, int startLine, int endLine) const;

#ifdef PHOTON_ENABLE_TREESITTER
    void registerLanguage(const std::string& name,
                          const std::vector<std::string>& extensions,
                          const TSLanguage* language);
    bool registerLanguageFromLibrary(const std::string& name,
                                     const std::vector<std::string>& extensions,
                                     const std::string& libraryPath,
                                     const std::string& symbolName);
#endif

private:
    struct Language {
        std::string name;
        std::vector<std::string> extensions;
#ifdef PHOTON_ENABLE_TREESITTER
        const TSLanguage* language = nullptr;
#endif
    };

    std::vector<Language> languages;

#ifdef PHOTON_ENABLE_TREESITTER
    const Language* languageForExtension(const std::string& ext) const;
    void collectSymbols(TSNode node,
                        const std::string& relPath,
                        const std::string& content,
                        std::vector<Symbol>& out) const;
    std::vector<void*> handles;
#endif
};
