#!/bin/bash
# 将项目根目录的说明类 Markdown 移入 docs/tutorials
# 在项目根目录执行: bash docs/tutorials/move_docs_here.sh

set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DST="$(cd "$(dirname "$0")" && pwd)"

files=(
  ARBITRARY_PATH_ANALYSIS.md
  CLEANUP_COMPLETE.md
  CONFIGURABLE_IGNORE_PATTERNS.md
  CONSTITUTION_EVOLUTION.md
  DEBUG_OPTIONS.md
  FEATURES.md
  FIXES_SUMMARY.md
  GLOBAL_PHOTON_SUPPORT.md
  INTEGRATION_COMPLETE.md
  LSP_OPTIMIZATION.md
  PATH_NORMALIZATION_FIX.md
  PATH_SEPARATOR_FIX.md
  PERFORMANCE_DEBUG_GUIDE.md
  READ_CODE_BLOCK_FIX.md
  README_REFACTOR.md
  REFACTOR_COMPLETE.md
  REFACTOR_STATUS.md
  REFACTOR_SUMMARY.md
  SEMANTIC_SEARCH_COMPLETE.md
  SEMANTIC_SEARCH_TOOL_COMPLETE.md
  STARTUP_OPTIMIZATION_COMPLETE.md
  SYMBOL_SCAN_FIX_COMPLETE.md
  SYMBOL_STRATEGY_DESIGN.md
  TOOL_INTELLIGENCE_COMPLETE.md
  TREESITTER_FIX_COMPLETE.md
  TUTORIALS_UPDATE_COMPLETE.md
  UTF8_FIX_COMPLETE.md
  VIEW_SYMBOL_DEPRECATION.md
  WINDOWS_MSVC_FIX.md
)

cd "$ROOT"
for f in "${files[@]}"; do
  [ -f "$f" ] && mv "$f" "$DST/" && echo "moved $f"
done
[ -f "文档整理完成.md" ] && mv "文档整理完成.md" "$DST/" && echo "moved 文档整理完成.md"
echo "Done."
