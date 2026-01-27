#!/bin/bash

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}开始构建 Photon (不使用 CMake)...${NC}"

ENABLE_TS="OFF"
if command -v python3 >/dev/null 2>&1; then
    ENABLE_TS=$(python3 - <<'PY'
import json
try:
    with open("config.json", "r", encoding="utf-8") as f:
        cfg = json.load(f)
    enabled = bool(cfg.get("agent", {}).get("enable_tree_sitter", False))
    print("ON" if enabled else "OFF")
except Exception:
    print("OFF")
PY
)
elif command -v python >/dev/null 2>&1; then
    ENABLE_TS=$(python - <<'PY'
import json
try:
    with open("config.json", "r", encoding="utf-8") as f:
        cfg = json.load(f)
    enabled = bool(cfg.get("agent", {}).get("enable_tree_sitter", False))
    print("ON" if enabled else "OFF")
except Exception:
    print("OFF")
PY
)
fi

echo -e "${GREEN}Tree-sitter: ${ENABLE_TS}${NC}"

# 设置编译器
CXX=g++
CC=gcc
if [[ "$OSTYPE" == "darwin"* ]]; then
    CXX=clang++
    CC=clang
    # macOS Homebrew 路径设置
    BREW_PREFIX=$(brew --prefix 2>/dev/null || echo "/opt/homebrew")
    
    # 检查核心依赖
    if [ ! -d "$BREW_PREFIX/opt/openssl@3" ] && [ ! -d "/usr/local/opt/openssl@3" ]; then
        echo -e "${RED}错误: 未找到 OpenSSL。请运行: brew install openssl@3${NC}"
        exit 1
    fi
    
    INCLUDES="-I$BREW_PREFIX/include -I./src"
    LIBS="-L$BREW_PREFIX/lib -lssl -lcrypto -lpthread"
else
    # Linux 路径设置
    # 检查核心依赖 (简单检查头文件)
    if [ ! -f "/usr/include/openssl/ssl.h" ]; then
        echo -e "${RED}错误: 未找到 OpenSSL 头文件。请运行: sudo apt install libssl-dev${NC}"
        exit 1
    fi
    
    INCLUDES="-I/usr/include -I./src"
    LIBS="-lssl -lcrypto -lpthread"
fi

# 编译参数
CXXFLAGS="-std=c++17 -O3 -Wall $INCLUDES"
TS_OBJS=""

if [ "$ENABLE_TS" = "ON" ]; then
    if [ ! -d "third_party/tree-sitter" ] || [ ! -d "third_party/tree-sitter-cpp" ] || [ ! -d "third_party/tree-sitter-python" ]; then
        echo -e "${RED}错误: 未找到 Tree-sitter 依赖，请确保 third_party 目录完整${NC}"
        exit 1
    fi

    TS_INCLUDES="-Ithird_party/tree-sitter/lib/include -Ithird_party/tree-sitter-cpp/bindings/c -Ithird_party/tree-sitter-python/bindings/c"
    CXXFLAGS="$CXXFLAGS $TS_INCLUDES -DPHOTON_ENABLE_TREESITTER"
    CFLAGS="-std=c11 -O3 -Wall $TS_INCLUDES"

    mkdir -p build/ts
    TS_SRCS=(
        "third_party/tree-sitter/lib/src/lib.c"
        "third_party/tree-sitter-cpp/src/parser.c"
        "third_party/tree-sitter-cpp/src/scanner.c"
        "third_party/tree-sitter-python/src/parser.c"
        "third_party/tree-sitter-python/src/scanner.c"
    )
    for SRC in "${TS_SRCS[@]}"; do
        OBJ="build/ts/${SRC//\//_}.o"
        $CC $CFLAGS -c "$SRC" -o "$OBJ" || exit 1
        TS_OBJS="$TS_OBJS $OBJ"
    done
fi

# 源文件列表
SRCS="src/core/main.cpp src/utils/FileManager.cpp src/utils/SymbolManager.cpp src/utils/RegexSymbolProvider.cpp src/utils/TreeSitterSymbolProvider.cpp src/core/LLMClient.cpp src/core/ContextManager.cpp src/mcp/MCPClient.cpp src/mcp/LSPClient.cpp src/mcp/InternalMCPClient.cpp"

# 执行编译
echo "正在执行: $CXX $SRCS $CXXFLAGS $LIBS $TS_OBJS -o photon"
$CXX $SRCS $CXXFLAGS $LIBS $TS_OBJS -o photon

if [ $? -eq 0 ]; then
    echo -e "${GREEN}构建成功! 可执行文件: ./photon${NC}"
else
    echo -e "${RED}构建失败，请检查是否安装了依赖库 (openssl, nlohmann-json)${NC}"
    exit 1
fi
