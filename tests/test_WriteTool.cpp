/**
 * 写入工具（apply_patch）与 Constitution 3.3 校验的集成测试。
 * 确保：1）ConstitutionValidator 对 apply_patch 的 files 校验正确；
 *       2）ApplyPatchTool 能正确按 files[]（content/edits）写入文件。
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
  std::string s;
  std::ifstream f(p);
  if (f) s.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return s;
}

// --- ConstitutionValidator: apply_patch 写入约束 (Section 3.3) ---

TEST(WriteTool, ConstitutionAcceptsValidDiffContent) {
  nlohmann::json args;
  args["files"] = nlohmann::json::array({
    nlohmann::json::object({
      {"path", "README.md"},
      {"content", "line1\nline2_added\nline3\n"}
    })
  });
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_TRUE(res.valid) << res.error;
}

TEST(WriteTool, ConstitutionRejectsMissingDiffContent) {
  nlohmann::json args;
  args["backup"] = true;
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("files") != std::string::npos);
}

TEST(WriteTool, ConstitutionRejectsEmptyDiffContent) {
  nlohmann::json args;
  args["files"] = nlohmann::json::array();
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("files") != std::string::npos || res.error.size() > 0);
}

TEST(WriteTool, ConstitutionRejectsDiffWithoutHunkHeaders) {
  nlohmann::json args;
  args["files"] = nlohmann::json::array({
    nlohmann::json::object({{"path", "x.txt"}})
  });
  auto res = ConstitutionValidator::validateToolCall("apply_patch", args);
  EXPECT_FALSE(res.valid);
  EXPECT_TRUE(res.error.find("content") != std::string::npos || res.error.find("edits") != std::string::npos);
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

  nlohmann::json args;
  args["files"] = nlohmann::json::array({
    nlohmann::json::object({
      {"path", "doc.txt"},
      {"edits", nlohmann::json::array({
        nlohmann::json::object({{"start_line", 2}, {"end_line", 2}, {"content", "B\nX\nY\n"}})
      })}
    })
  });
  args["backup"] = false;

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

  nlohmann::json args;
  args["files"] = nlohmann::json::array({
    nlohmann::json::object({
      {"path", "t.txt"},
      {"content", "new1\nold3\n"}
    })
  });
  args["backup"] = false;

  ApplyPatchTool tool(root.u8string(), false);
  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.value("success", false)) << res.dump(2);

  std::string content = readAll(f);
  EXPECT_TRUE(content.find("new1") != std::string::npos && content.find("old3") != std::string::npos) << content;
  EXPECT_TRUE(content.find("old1") == std::string::npos && content.find("old2") == std::string::npos) << content;
}
