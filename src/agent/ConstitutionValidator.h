#pragma once
#include <string>
#include <nlohmann/json.hpp>

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
    /**
     * @brief Section 3.1: IO Constraints
     * - File reads exceeding 500 lines are prohibited
     * - Reads must be scoped to explicit files and line ranges
     */
    static ValidationResult validateReadConstraints(const nlohmann::json& args) {
        if (!args.contains("file_path")) {
            return {false, "Read operation lacks explicit file path", "Section 3.1: IO Constraints"};
        }

        // Check line range
        if (args.contains("start_line") && args.contains("end_line")) {
            int start = args["start_line"].get<int>();
            int end = args["end_line"].get<int>();
            int lineCount = end - start + 1;
            
            if (lineCount > 500) {
                return {
                    false, 
                    "Read operation exceeds 500 line limit (" + std::to_string(lineCount) + " lines requested)",
                    "Section 3.1: IO Constraints"
                };
            }
        }
        // If no line range specified, it will read entire file - this should be caught at runtime
        // but we allow it here as CoreTools will handle the actual size check
        
        return {true, "", ""};
    }

    /**
     * @brief Section 3.3: Write Constraints (apply_patch)
     * - All writes go through apply_patch with unified diff (line-bounded, reversible).
     * - Full-file overwrites are prohibited; diff_content must describe bounded hunks.
     */
    static ValidationResult validateWriteConstraints(const nlohmann::json& args) {
        if (!args.contains("diff_content") || !args["diff_content"].is_string()) {
            return {false, "Write operation requires diff_content (unified diff string).", "Section 3.3: Write Constraints"};
        }
        std::string diff = args["diff_content"].get<std::string>();
        if (diff.empty()) {
            return {false, "diff_content must be non-empty.", "Section 3.3: Write Constraints"};
        }
        // Unified diff must contain at least one hunk header (line-bounded patch)
        if (diff.find("@@") == std::string::npos) {
            return {false, "diff_content must be a valid unified diff (contain @@ hunk headers).", "Section 3.3: Write Constraints"};
        }
        return {true, "", ""};
    }
};
