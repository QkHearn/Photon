#include "analysis/providers/RegexSymbolProvider.h"
#include <sstream>

std::vector<Symbol> RegexSymbolProvider::extractSymbols(const std::string& content, const std::string& relPath) const {
    std::vector<Symbol> symbols;
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    static const std::regex cppClass(R"raw((class|struct)\s+([A-Za-z0-9_]+))raw");
    static const std::regex cppFunc(R"raw(([A-Za-z0-9_<>, :*&]+)\s+([A-Za-z0-9_]+)\s*\()raw");
    static const std::regex pyDef(R"raw(^\s*(def|async def)\s+([A-Za-z0-9_]+))raw");
    static const std::regex pyClass(R"raw(^\s*class\s+([A-Za-z0-9_]+))raw");

    while (std::getline(stream, line)) {
        lineNum++;
        std::smatch match;
        if (std::regex_search(line, match, cppClass) || std::regex_search(line, match, pyClass)) {
            symbols.push_back({match[2].str(), "class", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, cppFunc)) {
            symbols.push_back({match[2].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, pyDef)) {
            symbols.push_back({match[2].str(), "function", "regex", relPath, lineNum, 0, line});
        }
    }

    return symbols;
}

bool RegexSymbolProvider::supportsExtension(const std::string& ext) const {
    return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".py";
}
