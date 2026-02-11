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
    static const std::regex tsClass(R"raw(^\s*(class|interface)\s+([A-Za-z0-9_]+))raw");
    static const std::regex tsFunc(R"raw(^\s*(function|async function)?\s+([A-Za-z0-9_]+)\s*\()raw");
    static const std::regex tsArrowFunc(R"raw(^\s*([A-Za-z0-9_]+)\s*:\s*\()raw");
    static const std::regex tsInterface(R"raw(^\s*interface\s+([A-Za-z0-9_]+))raw");
    static const std::regex tsType(R"raw(^\s*type\s+([A-Za-z0-9_]+)\s*=)raw");
    static const std::regex tsEnum(R"raw(^\s*enum\s+([A-Za-z0-9_]+))raw");
    static const std::regex etsClass(R"raw(^\s*(class|struct|interface)\s+([A-Za-z0-9_]+))raw");
    static const std::regex etsFunc(R"raw(^\s*function\s+([A-Za-z0-9_]+)\s*\()raw");
    static const std::regex etsArrowFunc(R"raw(^\s*([A-Za-z0-9_]+)\s*:\s*\()raw");

    while (std::getline(stream, line)) {
        lineNum++;
        std::smatch match;
        if (std::regex_search(line, match, cppClass) || std::regex_search(line, match, pyClass) || std::regex_search(line, match, tsClass) || std::regex_search(line, match, etsClass)) {
            symbols.push_back({match[2].str(), "class", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, cppFunc)) {
            symbols.push_back({match[2].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, pyDef)) {
            symbols.push_back({match[2].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, tsFunc)) {
            symbols.push_back({match[2].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, tsArrowFunc)) {
            symbols.push_back({match[1].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, tsInterface)) {
            symbols.push_back({match[1].str(), "interface", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, tsType)) {
            symbols.push_back({match[1].str(), "type", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, tsEnum)) {
            symbols.push_back({match[1].str(), "enum", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, etsFunc)) {
            symbols.push_back({match[1].str(), "function", "regex", relPath, lineNum, 0, line});
        } else if (std::regex_search(line, match, etsArrowFunc)) {
            symbols.push_back({match[1].str(), "function", "regex", relPath, lineNum, 0, line});
        }
    }

    return symbols;
}

bool RegexSymbolProvider::supportsExtension(const std::string& ext) const {
    return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".py" || ext == ".ts" || ext == ".tsx" || ext == ".js" || ext == ".jsx" || ext == ".ets";
}
