/**
 * 系统测试：理解需求 → 需求落地 → 验证
 *
 * 模拟完整流程：用 attempt 记录用户意图 → 按意图执行 apply_patch → 验证结果并更新 attempt。
 * 不依赖 LLM，仅验证工具链与工作流正确。
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
  std::ifstream f(p);
  if (!f.is_open()) return "";
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

TEST(SystemRequirementFlow, UnderstandRequirementThenLandAndVerify) {
  fs::path root = fs::temp_directory_path() / "photon_system_req_flow";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  AttemptTool attemptTool(root.u8string());
  ApplyPatchTool applyTool(root.u8string(), /*hasGit=*/false);

  // -------------------------------------------------------------------------
  // 1. 理解需求：将用户意图与读范围写入 attempt（模拟“理解需求”）
  // -------------------------------------------------------------------------
  const std::string userIntent = "在项目中新增 src/greeting.txt，内容为 Hello Photon，并验证文件存在且内容正确。";
  auto updateRes = attemptTool.execute({
    {"action", "update"},
    {"intent", userIntent},
    {"status", "in_progress"},
    {"read_scope", nlohmann::json::array({"src/"})}
  });
  ASSERT_FALSE(updateRes.contains("error")) << updateRes.dump(2);
  EXPECT_EQ(updateRes["attempt"]["intent"].get<std::string>(), userIntent);
  EXPECT_EQ(updateRes["attempt"]["status"].get<std::string>(), "in_progress");

  // -------------------------------------------------------------------------
  // 2. 需求落地：按意图执行 apply_patch（新增文件）
  // -------------------------------------------------------------------------
  const std::string diffNewFile =
      "diff --git a/src/greeting.txt b/src/greeting.txt\n"
      "new file mode 100644\n"
      "index 0000000..0000000\n"
      "--- /dev/null\n"
      "+++ b/src/greeting.txt\n"
      "@@ -0,0 +1,1 @@\n"
      "+Hello Photon\n";

  auto applyRes = applyTool.execute({
    {"diff_content", diffNewFile},
    {"backup", false},
    {"dry_run", false}
  });
  ASSERT_FALSE(applyRes.contains("error")) << applyRes.dump(2);
  ASSERT_TRUE(applyRes.value("success", false)) << applyRes.dump(2);

  // 记录已完成步骤
  attemptTool.execute({{"action", "update"}, {"step_done", "Created src/greeting.txt"}});
  attemptTool.execute({{"action", "update"}, {"affected_files", nlohmann::json::array({"src/greeting.txt"})}});

  // -------------------------------------------------------------------------
  // 3. 验证：检查文件存在且内容正确
  // -------------------------------------------------------------------------
  fs::path greetingPath = root / "src" / "greeting.txt";
  ASSERT_TRUE(fs::exists(greetingPath)) << "src/greeting.txt should exist";
  std::string content = readAll(greetingPath);
  EXPECT_TRUE(content.find("Hello Photon") != std::string::npos) << "content: " << content;

  // -------------------------------------------------------------------------
  // 4. 标记需求完成，更新 attempt 状态
  // -------------------------------------------------------------------------
  attemptTool.execute({{"action", "update"}, {"status", "done"}});
  auto getRes = attemptTool.execute({{"action", "get"}});
  ASSERT_FALSE(getRes.contains("error")) << getRes.dump(2);
  EXPECT_EQ(getRes["attempt"]["status"].get<std::string>(), "done");
  EXPECT_TRUE(getRes["attempt"].contains("steps_completed"));
  EXPECT_TRUE(getRes["attempt"]["steps_completed"].size() >= 1);

  // 清空 attempt，便于下一轮新需求
  attemptTool.execute({{"action", "clear"}});
  auto afterClear = attemptTool.execute({{"action", "get"}});
  EXPECT_TRUE(afterClear["attempt"].empty() || !afterClear["attempt"].contains("intent"));
}

TEST(SystemRequirementFlow, LandModifyExistingFileThenVerify) {
  fs::path root = fs::temp_directory_path() / "photon_system_req_flow2";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  // 预先存在一个文件
  fs::path target = root / "config.ini";
  {
    std::ofstream f(target);
    f << "[app]\nversion=1\n";
  }

  AttemptTool attemptTool(root.u8string());
  ApplyPatchTool applyTool(root.u8string(), false);

  // 理解需求：修改 config.ini，在 [app] 下增加 greeting=Photon
  attemptTool.execute({
    {"action", "update"},
    {"intent", "在 config.ini 的 [app] 下增加一行 greeting=Photon"},
    {"status", "in_progress"}
  });

  // 需求落地：unified diff 修改
  const std::string diffModify =
      "diff --git a/config.ini b/config.ini\n"
      "--- a/config.ini\n"
      "+++ b/config.ini\n"
      "@@ -1,2 +1,3 @@\n"
      " [app]\n"
      " version=1\n"
      "+greeting=Photon\n";

  auto applyRes = applyTool.execute({{"diff_content", diffModify}, {"backup", false}, {"dry_run", false}});
  ASSERT_FALSE(applyRes.contains("error")) << applyRes.dump(2);
  ASSERT_TRUE(applyRes.value("success", false)) << applyRes.dump(2);

  // 验证
  std::string content = readAll(target);
  EXPECT_TRUE(content.find("greeting=Photon") != std::string::npos) << content;
  EXPECT_TRUE(content.find("[app]") != std::string::npos && content.find("version=1") != std::string::npos) << content;

  attemptTool.execute({{"action", "update"}, {"status", "done"}, {"step_done", "Modified config.ini"}});
  auto getRes = attemptTool.execute({{"action", "get"}});
  EXPECT_EQ(getRes["attempt"]["status"].get<std::string>(), "done");
}
