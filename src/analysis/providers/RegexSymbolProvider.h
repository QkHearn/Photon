#pragma once
#include "analysis/SymbolManager.h"
#include <regex>

class RegexSymbolProvider : public ISymbolProvider {
public:
    std::vector<Symbol> extractSymbols(const std::string& content, const std::string& relPath) const override;
    bool supportsExtension(const std::string& ext) const override;
};
