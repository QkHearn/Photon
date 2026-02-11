/**
 * 写入工具（apply_patch）与 Constitution 3.3 校验的集成测试。
 * 确保：1）ConstitutionValidator 对 apply_patch 的 diff_content 校验正确；
 *       2）ApplyPatchTool 能正确应用 unified diff 并写入文件。
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "agent/ConstitutionValidator.h"
#include "tools/CoreTools.h"

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
  std::ifstream f(p);
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return s;
}

// --- ConstitutionValidator: apply_patch 写入约束 (Section 3.3) ---

TEST(WriteTool, ConstitutionAcceptsValidDiffContent) {
  nlohmann::json args;
  args["diff_content"] =
      "diff --git a/README.md b/README.md\n"
      "--- a/README.md\n"
      "+++ b/README.md\n"
      "@@ -1,3 +1,5 @@\n"
      " line1\n"
      "+line2_added\n"
      " line3\n";
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_TRUE(res.valid) << res.error;
}

TEST(WriteTool, ConstitutionRejectsMissingDiffContent) {
  nlohmann::json args;
  args["backup"] = true;
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("diff_content") != std::string::npos);
}

TEST(WriteTool, ConstitutionRejectsEmptyDiffContent) {
  nlohmann::json args;
  args["diff_content"] = "";
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("non-empty") != std::string::npos || res.error.size() > 0);
}

TEST(WriteTool, ConstitutionRejectsDiffWithoutHunkHeaders) {
  nlohmann::json args;
  args["diff_content"] = "just some text\nno hunk headers here\n";
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("@@") != std::string::npos || res.error.find("unified diff") != std::string::npos);
}

// --- ApplyPatchTool: 实际写入行为 ---

TEST(WriteTool, ApplyPatchInsertsLinesAndWritesFile) {
  fs::path root = fs::temp_directory_path() / "photon_write_tool_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  fs::path f = root / "doc.txt";
  {
    std::ofstream out(f);
    out << "A\nB\nC\n";
  }

  // 在 B 后插入两行
  const std::string diff =
      "diff --git a/doc.txt b/doc.txt\n"
      "--- a/doc.txt\n"
      "+++ b/doc.txt\n"
      "@@ -1,3 +1,5 @@\n"
      " A\n"
      " B\n"
      "+X\n"
      "+Y\n"
      " C\n";

  nlohmann::json args;
  args["diff_content"] = diff;
  args["backup"] = false;
  args["dry_run"] = false;

  ApplyPatchTool tool(root.u8string(), false);
  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.value("success", false)) << res.dump(2);

  std::string content = readAll(f);
  EXPECT_TRUE(content == "A\nB\nX\nY\nC\n" || content == "A\nB\nX\nY\nC") << "content:\n" << content;
}

TEST(WriteTool, ApplyPatchReplaceAndDeleteLines) {
  fs::path root = fs::temp_directory_path() / "photon_write_tool_test2";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  fs::path f = root / "t.txt";
  {
    std::ofstream out(f);
    out << "old1\nold2\nold3\n";
  }

  const std::string diff =
      "diff --git a/t.txt b/t.txt\n"
      "--- a/t.txt\n"
      "+++ b/t.txt\n"
      "@@ -1,3 +1,2 @@\n"
      "-old1\n"
      "-old2\n"
      "+new1\n"
      " old3\n";

  ApplyPatchTool tool(root.u8string(), false);
  nlohmann::json args;
  args["diff_content"] = diff;
  args["backup"] = false;
  args["dry_run"] = false;

  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.value("success", false)) << res.dump(2);

  std::string content = readAll(f);
  EXPECT_TRUE(content.find("new1") != std::string::npos && content.find("old3") != std::string::npos) << content;
  EXPECT_TRUE(content.find("old1") == std::string::npos && content.find("old2") == std::string::npos) << content;
}
