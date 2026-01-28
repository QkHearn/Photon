#!/bin/bash

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo -e "${GREEN}开始构建 Photon...${NC}"

# 1. 从 config.json 自动检测 Tree-sitter 是否启用
ENABLE_TS="OFF"
if [ -f "config.json" ]; then
    if command -v python3 >/dev/null 2>&1; then
        ENABLE_TS=$(python3 -c "import json; f=open('config.json'); d=json.load(f); print('ON' if d.get('agent', {}).get('enable_tree_sitter', False) else 'OFF')" 2>/dev/null || echo "OFF")
    elif command -v python >/dev/null 2>&1; then
        ENABLE_TS=$(python -c "import json; f=open('config.json'); d=json.load(f); print('ON' if d.get('agent', {}).get('enable_tree_sitter', False) else 'OFF')" 2>/dev/null || echo "OFF")
    fi
fi

echo -e "${GREEN}检测到配置: Tree-sitter = ${ENABLE_TS}${NC}"

# 2. 尝试使用 CMake 构建
if command -v cmake >/dev/null 2>&1; then
    echo -e "${GREEN}检测到 CMake，使用 CMake 构建系统...${NC}"
    mkdir -p build
    cd build || exit 1
    
    cmake .. -DCMAKE_BUILD_TYPE=Release -DPHOTON_ENABLE_TREESITTER=${ENABLE_TS}
    if [ $? -eq 0 ]; then
        cmake --build . --config Release
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}CMake 构建成功!${NC}"
            cd ..
            if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
                cp build/photon.exe photon.exe 2>/dev/null || cp build/Release/photon.exe photon.exe 2>/dev/null
            else
                ln -sf build/photon photon
            fi
            echo -e "${GREEN}构建产物已就绪: ./photon${NC}"
            exit 0
        fi
    fi
    echo -e "${YELLOW}CMake 构建失败，尝试回退到手动编译逻辑...${NC}"
    cd ..
else
    echo -e "${YELLOW}未检测到 CMake，使用手动编译逻辑...${NC}"
fi

# 3. 手动编译逻辑 (Fallback)
# 设置编译器
CXX=g++
CC=gcc
if [[ "$OSTYPE" == "darwin"* ]]; then
    CXX=clang++
    CC=clang
    BREW_PREFIX=$(brew --prefix 2>/dev/null || echo "/opt/homebrew")
    INCLUDES="-I$BREW_PREFIX/include -I./src"
    LIBS="-L$BREW_PREFIX/lib -lssl -lcrypto -lpthread"
else
    INCLUDES="-I/usr/include -I./src"
    LIBS="-lssl -lcrypto -lpthread"
fi

CXXFLAGS="-std=c++17 -O3 -Wall $INCLUDES"
TS_OBJS=""

if [ "$ENABLE_TS" = "ON" ]; then
    echo -e "${GREEN}正在编译 Tree-sitter 依赖...${NC}"
    TS_INCLUDES="-Ithird_party/tree-sitter/lib/include -Ithird_party/tree-sitter-cpp/bindings/c -Ithird_party/tree-sitter-python/bindings/c -Ithird_party/tree-sitter-typescript/bindings/c -Ithird_party/tree-sitter-arkts/bindings/c"
    CXXFLAGS="$CXXFLAGS $TS_INCLUDES -DPHOTON_ENABLE_TREESITTER"
    CFLAGS="-std=c11 -O3 -Wall $TS_INCLUDES"

    mkdir -p build/ts
    TS_SRCS=(
        "third_party/tree-sitter/lib/src/lib.c"
        "third_party/tree-sitter-cpp/src/parser.c"
        "third_party/tree-sitter-cpp/src/scanner.c"
        "third_party/tree-sitter-python/src/parser.c"
        "third_party/tree-sitter-python/src/scanner.c"
        "third_party/tree-sitter-typescript/typescript/src/parser.c"
        "third_party/tree-sitter-typescript/typescript/src/scanner.c"
        "third_party/tree-sitter-arkts/src/parser.c"
    )
    for SRC in "${TS_SRCS[@]}"; do
        OBJ="build/ts/${SRC//\//_}.o"
        # TypeScript scanner needs common/ include
        CURRENT_CFLAGS="$CFLAGS"
        if [[ "$SRC" == *"tree-sitter-typescript"* ]]; then
            CURRENT_CFLAGS="$CFLAGS -Ithird_party/tree-sitter-typescript/common -Ithird_party/tree-sitter-typescript/typescript/src"
        fi
        $CC $CURRENT_CFLAGS -c "$SRC" -o "$OBJ" || { echo -e "${RED}Tree-sitter 编译失败: $SRC${NC}"; exit 1; }
        TS_OBJS="$TS_OBJS $OBJ"
    done
fi

# 源文件列表
SRCS="src/core/main.cpp src/utils/FileManager.cpp src/utils/SymbolManager.cpp src/utils/SemanticManager.cpp src/utils/RegexSymbolProvider.cpp src/utils/TreeSitterSymbolProvider.cpp src/core/LLMClient.cpp src/core/ContextManager.cpp src/mcp/MCPClient.cpp src/mcp/LSPClient.cpp src/mcp/InternalMCPClient.cpp"

echo -e "${GREEN}正在手动编译 Photon...${NC}"
$CXX $SRCS $CXXFLAGS $LIBS $TS_OBJS -o photon

if [ $? -eq 0 ]; then
    echo -e "${GREEN}手动构建成功! 可执行文件: ./photon${NC}"
else
    echo -e "${RED}构建失败！请检查依赖库是否安装。${NC}"
    exit 1
fi
