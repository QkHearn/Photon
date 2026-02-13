#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <vector>

/**
 * @brief Photon Agent Constitution v2.0 Validator
 * 
 * Enforces hard constraints defined in the Constitution.
 * Violations result in execution failure (not warnings).
 */
class ConstitutionValidator {
public:
    struct ValidationResult {
        bool valid;
        std::string error;
        std::string constraint;  // Which constraint was violated
    };

    /**
     * @brief Validate tool call against Constitution constraints
     * @param toolName Tool being invoked
     * @param args Tool arguments
     * @return Validation result (valid=false triggers abort)
     */
    static ValidationResult validateToolCall(const std::string& toolName, const nlohmann::json& args) {
        // 3.1 IO Constraints
        if (toolName == "read_code_block") {
            return validateReadConstraints(args);
        }
        
        // 3.3 Write Constraints
        if (toolName == "apply_patch") {
            return validateWriteConstraints(args);
        }
        
        return {true, "", ""};
    }

private:
    /** Files with these extensions typically have no symbol index; allow full-file read (no 500-line cap). */
    static bool isLikelyNoSymbolFile(const std::string& filePath) {
        std::string path = filePath;
        size_t dot = path.rfind('.');
        if (dot == std::string::npos) return false;
        std::string ext = path.substr(dot);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        static const std::vector<std::string> noSymbolExt = {
            ".json", ".md", ".yml", ".yaml", ".txt", ".toml", ".xml", ".html", ".htm",
            ".cmake", ".lock", ".ini", ".cfg", ".conf", ".env", ".gitignore", ".cursorignore"
        };
        return std::find(noSymbolExt.begin(), noSymbolExt.end(), ext) != noSymbolExt.end();
    }

    /** @return true if item has symbol_name or start_line/end_line (required scope for read). */
    static bool hasReadScope(const nlohmann::json& item) {
        if (item.contains("symbol_name") && !item["symbol_name"].is_null()) {
            if (item["symbol_name"].is_string() && !item["symbol_name"].get<std::string>().empty()) return true;
        }
        if (item.contains("start_line") || item.contains("end_line")) return true;
        return false;
    }

    /**
     * @brief Section 3.1: IO Constraints
     * - Single read: file_path required; batch read: requests[] with each item having file_path
     * - Each read must specify scope: symbol_name OR start_line/end_line (line numbers)
     * - File reads exceeding 500 lines (per range) are prohibited
     */
    static ValidationResult validateReadConstraints(const nlohmann::json& args) {
        if (args.contains("requests") && args["requests"].is_array() && !args["requests"].empty()) {
            for (const auto& req : args["requests"]) {
                if (!req.is_object() || !req.contains("file_path")) {
                    return {false, "Each request must have file_path.", "Section 3.1: IO Constraints"};
                }
                if (!hasReadScope(req)) {
                    return {false, "Each read must include symbol_name or start_line/end_line (line scope).", "Section 3.1: IO Constraints"};
                }
                if (req.contains("start_line") && req.contains("end_line")) {
                    std::string fp = req.contains("file_path") && req["file_path"].is_string() ? req["file_path"].get<std::string>() : "";
                    if (!isLikelyNoSymbolFile(fp)) {
                        int start = req["start_line"].get<int>();
                        int end = req["end_line"].get<int>();
                        int lineCount = end - start + 1;
                        if (lineCount > 500) {
                            return {false, "Read operation exceeds 500 line limit (" + std::to_string(lineCount) + " lines).", "Section 3.1: IO Constraints"};
                        }
                    }
                }
            }
            return {true, "", ""};
        }
        if (!args.contains("file_path")) {
            return {false, "Read operation lacks explicit file path (use file_path or requests[].file_path).", "Section 3.1: IO Constraints"};
        }
        if (!hasReadScope(args)) {
            return {false, "Read must include symbol_name or start_line/end_line (line scope).", "Section 3.1: IO Constraints"};
        }
        if (args.contains("start_line") && args.contains("end_line")) {
            std::string fp = args["file_path"].is_string() ? args["file_path"].get<std::string>() : "";
            if (!isLikelyNoSymbolFile(fp)) {
                int start = args["start_line"].get<int>();
                int end = args["end_line"].get<int>();
                int lineCount = end - start + 1;
                if (lineCount > 500) {
                    return {false, "Read operation exceeds 500 line limit (" + std::to_string(lineCount) + " lines requested).", "Section 3.1: IO Constraints"};
                }
            }
        }
        return {true, "", ""};
    }

    /**
     * @brief Section 3.3: Write Constraints (apply_patch)
     * - apply_patch accepts files[]: each item has path and content (full file) or edits (line-based).
     */
    static ValidationResult validateWriteConstraints(const nlohmann::json& args) {
        if (!args.contains("files") || !args["files"].is_array() || args["files"].empty()) {
            return {false, "apply_patch requires non-empty files array (path + content or edits).", "Section 3.3: Write Constraints"};
        }
        for (const auto& item : args["files"]) {
            if (!item.is_object() || !item.contains("path")) {
                return {false, "Each file entry must have path.", "Section 3.3: Write Constraints"};
            }
            if (!item.contains("content") && !item.contains("edits")) {
                return {false, "Each file entry must have content (full) or edits (line-based).", "Section 3.3: Write Constraints"};
            }
            if (item.contains("edits") && item["edits"].is_array()) {
                for (const auto& ed : item["edits"]) {
                    if (!ed.is_object() || !ed.contains("start_line") || !ed.contains("content")) {
                        return {false, "Each edit must have start_line and content (start_line is required).", "Section 3.3: Write Constraints"};
                    }
                }
            }
        }
        return {true, "", ""};
    }
};
