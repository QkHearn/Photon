#!/usr/bin/env bash
# 一键运行「理解需求 → 需求落地 → 验证」系统测试
set -e
cd "$(dirname "$0")"
# 先构建，确保 agent_tests 包含 test_SystemRequirementFlow.cpp（否则会跑 0 tests）
echo "构建 agent_tests..."
./build.sh
./build/agent_tests --gtest_filter="SystemRequirementFlow.*" "$@"
