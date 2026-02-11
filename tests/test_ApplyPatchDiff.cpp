#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
  std::ifstream f(p);
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return s;
}

TEST(ApplyPatchTool, AppliesUnifiedDiffToFile_NoGit) {
  // Create a temporary project root.
  fs::path root = fs::temp_directory_path() / fs::path("photon_apply_patch_test_root");
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  // Create a file to patch.
  fs::path file = root / "a.txt";
  {
    std::ofstream out(file);
    out << "line1\n";
    out << "line2\n";
    out << "line3\n";
  }

  // Unified diff that replaces line2 -> LINE2.
  const std::string diff =
      "diff --git a/a.txt b/a.txt\n"
      "index 0000000..0000000 100644\n"
      "--- a/a.txt\n"
      "+++ b/a.txt\n"
      "@@ -1,3 +1,3 @@\n"
      " line1\n"
      "-line2\n"
      "+LINE2\n"
      " line3\n";

  ApplyPatchTool tool(root.u8string(), /*hasGit=*/false);
  nlohmann::json args;
  args["diff_content"] = diff;
  args["backup"] = true;
  args["dry_run"] = false;

  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.value("success", false)) << res.dump(2);

  const std::string content = readAll(file);
  // Our patch writer may omit the final trailing newline.
  EXPECT_TRUE(content == "line1\nLINE2\nline3\n" || content == "line1\nLINE2\nline3")
      << "Unexpected file content:\n" << content;
}

