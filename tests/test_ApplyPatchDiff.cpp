#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return s;
}

TEST(ApplyPatchTool, AppliesUnifiedDiffToFile_NoGit) {
  fs::path root = fs::temp_directory_path() / fs::path("photon_apply_patch_test_root");
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  fs::path file = root / "a.txt";
  {
    std::ofstream out(file, std::ios::binary);
    out << "line1\n";
    out << "line2\n";
    out << "line3\n";
  }

  // 新 API: files[] 每项 path + content 或 edits。此处用 edits 替换第 2 行为 LINE2
  ApplyPatchTool tool(root.u8string(), /*hasGit=*/false);
  nlohmann::json args;
  args["files"] = nlohmann::json::array({
    nlohmann::json::object({
      {"path", "a.txt"},
      {"edits", nlohmann::json::array({
        nlohmann::json::object({{"start_line", 2}, {"end_line", 2}, {"content", "LINE2\n"}})
      })}
    })
  });
  args["backup"] = true;

  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.value("success", false)) << res.dump(2);

  const std::string content = readAll(file);
  EXPECT_TRUE(content == "line1\nLINE2\nline3\n" || content == "line1\nLINE2\nline3")
      << "Unexpected file content:\n" << content;
}
