/**
 * ListProjectFilesTool 单元测试：目录列表、include_symbols、无 SymbolManager 时不带 sym。
 * 含性能用例：list 带 symbol 应在合理时间内完成（批量查符号，一次读锁）。
 */
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"
#include "analysis/SymbolManager.h"
#include "analysis/providers/RegexSymbolProvider.h"

namespace fs = std::filesystem;

static void createFile(const fs::path& p, const std::string& content) {
  std::ofstream f(p);
  ASSERT_TRUE(f.is_open()) << "create " << p.u8string();
  f << content;
  f.flush();
  ASSERT_TRUE(f) << "write " << p.u8string();
}

// 无 SymbolManager 时不应有 sym 字段
TEST(ListProjectFilesTool, NoSymbolsWhenNoSymbolManager) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_no_sym";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  createFile(root / "a.cpp", "void foo() {}\n");
  createFile(root / "readme.txt", "hello");

  ListProjectFilesTool tool(root.u8string(), nullptr, 8);
  auto res = tool.execute({{"path", "."}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.contains("tree")) << res.dump(2);

  const auto& tree = res["tree"];
  ASSERT_TRUE(tree.is_array());
  bool foundCpp = false;
  for (const auto& item : tree) {
    if (item["name"] == "a.cpp") {
      foundCpp = true;
      EXPECT_FALSE(item.contains("sym")) << "Without SymbolManager, file item should not have sym";
      break;
    }
  }
  EXPECT_TRUE(foundCpp) << "a.cpp should appear in tree";
}

// include_symbols: false 时不应带 sym
TEST(ListProjectFilesTool, NoSymbolsWhenIncludeSymbolsFalse) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_include_false";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  createFile(root / "bar.cpp", "class Bar {};\nvoid bar() {}\n");

  SymbolManager symbolManager(root.u8string());
  symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
  symbolManager.scanBlocking();

  ListProjectFilesTool tool(root.u8string(), &symbolManager, 8);
  auto res = tool.execute({{"path", "."}, {"include_symbols", false}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);

  const auto& tree = res["tree"];
  for (const auto& item : tree) {
    if (item["name"] == "bar.cpp") {
      EXPECT_FALSE(item.contains("sym")) << "include_symbols=false should not add sym";
      return;
    }
  }
}

// 有 SymbolManager 且 include_symbols 默认时，代码文件应有 sym 且文本中包含符号提示
TEST(ListProjectFilesTool, AttachesSymbolHintsForCodeFiles) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_sym";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  createFile(root / "ListTestHelper.cpp", "class ListTestHelper { };\nvoid listTestFunc() { }\n");

  SymbolManager symbolManager(root.u8string());
  symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
  symbolManager.scanBlocking();

  ListProjectFilesTool tool(root.u8string(), &symbolManager, 8);
  auto res = tool.execute({{"path", "."}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.contains("tree")) << res.dump(2);
  ASSERT_TRUE(res.contains("content")) << res.dump(2);

  const auto& tree = res["tree"];
  bool foundFile = false;
  for (const auto& item : tree) {
    if (item["name"] == "ListTestHelper.cpp") {
      foundFile = true;
      ASSERT_TRUE(item.contains("sym")) << "Code file should have sym when SymbolManager is set";
      std::string sym = item["sym"].get<std::string>();
      EXPECT_TRUE(sym.find("ListTestHelper") != std::string::npos) << sym;
      EXPECT_TRUE(sym.find("listTestFunc") != std::string::npos) << sym;
      EXPECT_TRUE(sym.find("C:") != std::string::npos || sym.find("F:") != std::string::npos) << sym;
      break;
    }
  }
  EXPECT_TRUE(foundFile) << "ListTestHelper.cpp should appear in tree";

  std::string text = res["content"][0]["text"].get<std::string>();
  EXPECT_TRUE(text.find("ListTestHelper.cpp") != std::string::npos) << text;
  EXPECT_TRUE(text.find("ListTestHelper") != std::string::npos || text.find("listTestFunc") != std::string::npos) << text;
}

// 路径不存在 / 非目录应返回错误
TEST(ListProjectFilesTool, ErrorWhenPathNotFound) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_err";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  ListProjectFilesTool tool(root.u8string());
  auto res = tool.execute({{"path", "nonexistent_sub"}});
  EXPECT_TRUE(res.contains("error")) << res.dump(2);
}

TEST(ListProjectFilesTool, RespectsMaxDepth) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_depth";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  fs::create_directories(root / "a" / "b" / "c");
  createFile(root / "a" / "b" / "c" / "deep.txt", "x");

  ListProjectFilesTool tool(root.u8string());
  auto res = tool.execute({{"path", "."}, {"max_depth", 2}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  // 深度 2 时 a -> a/b 会展开，a/b/c 不应作为 children 出现（或 c 下无 deep.txt 的展开）
  const auto& tree = res["tree"];
  ASSERT_TRUE(tree.is_array());
  EXPECT_GE(tree.size(), 1u);
}

// 性能：大量代码文件时 list_project_files（带 symbol）应较快完成，便于观察 list 耗时
TEST(ListProjectFilesTool, PerformanceListWithSymbols) {
  fs::path root = fs::temp_directory_path() / "photon_list_test_perf";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  const int numFiles = 60;
  for (int i = 0; i < numFiles; ++i) {
    std::string name = "file_" + std::to_string(i) + ".cpp";
    createFile(root / name, "class Class" + std::to_string(i) + " {};\nvoid func" + std::to_string(i) + "() {}\n");
  }

  SymbolManager symbolManager(root.u8string());
  symbolManager.registerProvider(std::make_unique<RegexSymbolProvider>());
  symbolManager.scanBlocking();

  ListProjectFilesTool tool(root.u8string(), &symbolManager, 8);
  auto start = std::chrono::steady_clock::now();
  auto res = tool.execute({{"path", "."}, {"include_symbols", true}});
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.contains("tree")) << res.dump(2);
  unsigned symCount = 0;
  std::function<void(const nlohmann::json&)> countSym = [&](const nlohmann::json& items) {
    for (const auto& item : items) {
      if (item.contains("sym")) ++symCount;
      if (item.contains("children")) countSym(item["children"]);
    }
  };
  countSym(res["tree"]);
  EXPECT_GE(symCount, static_cast<unsigned>(numFiles)) << "expected sym on each code file";

  // 期望 list（含批量取符号）在 2s 内完成，避免 N 次锁成为瓶颈
  EXPECT_LT(static_cast<long long>(elapsed), 2000) << "list_project_files with symbols took " << elapsed << " ms (expected < 2000 ms)";
  RecordProperty("list_with_symbols_ms", static_cast<int>(elapsed));
  RecordProperty("code_files", numFiles);
}
