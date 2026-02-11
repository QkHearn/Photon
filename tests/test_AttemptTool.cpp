/**
 * AttemptTool 单元测试：get/update/clear 当前用户意图与任务状态（.photon/current_attempt.json）。
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

#include "tools/CoreTools.h"

namespace fs = std::filesystem;

TEST(AttemptTool, GetReturnsEmptyWhenNoAttempt) {
  fs::path root = fs::temp_directory_path() / "photon_attempt_test_empty";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  fs::path photonDir = root / ".photon";
  if (fs::exists(photonDir)) fs::remove_all(photonDir, ec);

  AttemptTool tool(root.u8string());
  nlohmann::json args;
  args["action"] = "get";

  auto res = tool.execute(args);
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res.contains("attempt")) << res.dump(2);
  EXPECT_TRUE(res["attempt"].is_object());
  EXPECT_TRUE(res["attempt"].empty() || !res["attempt"].contains("intent"));
}

TEST(AttemptTool, UpdateCreatesAttemptFile) {
  fs::path root = fs::temp_directory_path() / "photon_attempt_test_update";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);
  fs::path attemptFile = root / ".photon" / "current_attempt.json";
  if (fs::exists(attemptFile)) fs::remove(attemptFile, ec);

  AttemptTool tool(root.u8string());

  nlohmann::json updateArgs;
  updateArgs["action"] = "update";
  updateArgs["intent"] = "Add retry to apply_patch";
  updateArgs["status"] = "in_progress";
  updateArgs["read_scope"] = nlohmann::json::array({"src/tools/CoreTools.cpp"});

  auto updateRes = tool.execute(updateArgs);
  ASSERT_FALSE(updateRes.contains("error")) << updateRes.dump(2);
  ASSERT_TRUE(updateRes.contains("attempt")) << updateRes.dump(2);
  EXPECT_EQ(updateRes["attempt"]["intent"], "Add retry to apply_patch");
  EXPECT_EQ(updateRes["attempt"]["status"], "in_progress");
  EXPECT_TRUE(updateRes["attempt"].contains("read_scope"));
  EXPECT_TRUE(updateRes["attempt"].contains("created_at"));
  EXPECT_TRUE(updateRes["attempt"].contains("updated_at"));

  ASSERT_TRUE(fs::exists(attemptFile)) << "current_attempt.json should be created";

  auto getRes = tool.execute({{"action", "get"}});
  ASSERT_FALSE(getRes.contains("error")) << getRes.dump(2);
  EXPECT_EQ(getRes["attempt"]["intent"], "Add retry to apply_patch");
}

TEST(AttemptTool, UpdateAppendsStepDone) {
  fs::path root = fs::temp_directory_path() / "photon_attempt_test_steps";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  AttemptTool tool(root.u8string());
  tool.execute({{"action", "update"}, {"intent", "Task"}, {"status", "in_progress"}});
  tool.execute({{"action", "update"}, {"step_done", "Located execute()"}});
  tool.execute({{"action", "update"}, {"step_done", "Applied diff"}});

  auto res = tool.execute({{"action", "get"}});
  ASSERT_FALSE(res.contains("error")) << res.dump(2);
  ASSERT_TRUE(res["attempt"].contains("steps_completed")) << res.dump(2);
  auto& steps = res["attempt"]["steps_completed"];
  ASSERT_TRUE(steps.is_array());
  EXPECT_GE(steps.size(), 2u);
  bool has1 = false, has2 = false;
  for (const auto& s : steps) {
    if (s.get<std::string>() == "Located execute()") has1 = true;
    if (s.get<std::string>() == "Applied diff") has2 = true;
  }
  EXPECT_TRUE(has1) << "steps_completed should contain 'Located execute()'";
  EXPECT_TRUE(has2) << "steps_completed should contain 'Applied diff'";
}

TEST(AttemptTool, ClearRemovesAttempt) {
  fs::path root = fs::temp_directory_path() / "photon_attempt_test_clear";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root);

  AttemptTool tool(root.u8string());
  tool.execute({{"action", "update"}, {"intent", "Temporary task"}});
  auto before = tool.execute({{"action", "get"}});
  EXPECT_FALSE(before["attempt"].empty());

  auto clearRes = tool.execute({{"action", "clear"}});
  ASSERT_FALSE(clearRes.contains("error")) << clearRes.dump(2);

  auto after = tool.execute({{"action", "get"}});
  ASSERT_FALSE(after.contains("error")) << after.dump(2);
  EXPECT_TRUE(after["attempt"].empty() || !after["attempt"].contains("intent"));
}
