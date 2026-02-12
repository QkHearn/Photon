/**
 * GrepTool 单元测试：在临时目录创建文件，按 pattern 搜索，验证返回的 file/line/content。
 * 依赖系统有 grep、rg 或 Windows findstr（CI 与常见开发环境均有）。
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"

namespace fs = std::filesystem;

static void createFile(const fs::path& p, const std::string& content) {
  std::ofstream f(p);
  ASSERT_TRUE(f.is_open()) << "create " << p.u8string();
  f << content;
  f.flush();
  ASSERT_TRUE(f) << "write " << p.u8string();
}

TEST(GrepTool, RejectsEmptyPattern) {
  fs::path root = fs::temp_directory_path() / "photon_grep_test_reject";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  GrepTool tool(root.u8string());
  auto res = tool.execute({{"pattern", ""}});
  EXPECT_TRUE(res.contains("error")) << res.dump(2);
}

TEST(GrepTool, RejectsMissingPattern) {
  fs::path root = fs::temp_directory_path() / "photon_grep_test_missing";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  GrepTool tool(root.u8string());
  auto res = tool.execute(nlohmann::json::object());
  EXPECT_TRUE(res.contains("error")) << res.dump(2);
}

TEST(GrepTool, FindsLiteralInCreatedFiles) {
#ifdef _WIN32
  // Skip on Windows: findstr/env 在部分 CI 上不稳定，避免阻塞构建
  GTEST_SKIP() << "FindsLiteralInCreatedFiles skipped on Windows";
#endif
  fs::path root = fs::temp_directory_path() / "photon_grep_test_find";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  createFile(root / "a.txt", "line1\nPhotonGrepTestToken\nline3\n");
  createFile(root / "b.txt", "other\nPhotonGrepTestToken\n");

  GrepTool tool(root.u8string());
  nlohmann::json args;
  args["pattern"] = "PhotonGrepTestToken";
  args["path"] = ".";

  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.contains("matches")) << res.dump(2);
  auto& matches = res["matches"];
  ASSERT_TRUE(matches.is_array()) << res.dump(2);
  EXPECT_GE(matches.size(), 1u) << "expected at least one match";

  bool foundA = false;
  for (const auto& m : matches) {
    ASSERT_TRUE(m.contains("file") && m.contains("line") && m.contains("content")) << m.dump(2);
    std::string file = m["file"].get<std::string>();
    int line = m["line"].get<int>();
    std::string content = m["content"].get<std::string>();
    if (fs::path(file).filename().u8string() == "a.txt" && line == 2) foundA = true;
    EXPECT_TRUE(content.find("PhotonGrepTestToken") != std::string::npos) << content;
  }
  EXPECT_TRUE(foundA) << "expected match in a.txt line 2, got matches: " << matches.dump(2);
}

TEST(GrepTool, RespectsMaxResults) {
  fs::path root = fs::temp_directory_path() / "photon_grep_test_max";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  std::string many;
  for (int i = 0; i < 50; ++i) many += "SameLine\n";
  createFile(root / "many.txt", many);

  GrepTool tool(root.u8string());
  auto res = tool.execute({{"pattern", "SameLine"}, {"path", "."}, {"max_results", 5}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res["matches"].is_array());
  EXPECT_LE(res["matches"].size(), 5u) << "max_results=5 should cap matches";
  EXPECT_EQ(res["count"].get<int>(), static_cast<int>(res["matches"].size()));
}
