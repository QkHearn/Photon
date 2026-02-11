#!/bin/bash

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo -e "${GREEN}开始构建 Photon...${NC}"

# 1. 从 config.json 自动检测 Tree-sitter 是否启用（与 CI 一致）
ENABLE_TS="OFF"
if [ -f "config.json" ]; then
    if command -v python3 >/dev/null 2>&1; then
        ENABLE_TS=$(python3 -c "import json; f=open('config.json'); d=json.load(f); print('ON' if d.get('agent', {}).get('enable_tree_sitter', False) else 'OFF')" 2>/dev/null || echo "OFF")
    elif command -v python >/dev/null 2>&1; then
        ENABLE_TS=$(python -c "import json; f=open('config.json'); d=json.load(f); print('ON' if d.get('agent', {}).get('enable_tree_sitter', False) else 'OFF')" 2>/dev/null || echo "OFF")
    fi
fi

echo -e "${GREEN}检测到配置: Tree-sitter = ${ENABLE_TS}${NC}"

# 2. 使用 CMake 构建（与 .github/workflows/c-cpp.yml 保持一致）
if ! command -v cmake >/dev/null 2>&1; then
    echo -e "${RED}未检测到 CMake，请先安装 cmake.${NC}"
    exit 1
fi

mkdir -p build
cd build || exit 1

echo -e "${GREEN}使用 CMake 构建 (Release, PHOTON_ENABLE_TREESITTER=${ENABLE_TS})...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DPHOTON_ENABLE_TREESITTER=${ENABLE_TS}
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 配置失败，请检查依赖（nlohmann-json, OpenSSL, gtest 等）是否安装。${NC}"
    exit 1
fi

cmake --build . --config Release
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 构建失败，请修复编译错误后重试。${NC}"
    exit 1
fi

cd ..
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    cp build/photon.exe photon.exe 2>/dev/null || cp build/Release/photon.exe photon.exe 2>/dev/null
else
    ln -sf build/photon photon
fi

echo -e "${GREEN}构建成功，产物: ./build/photon（以及方便调用的 ./photon 链接）${NC}"
exit 0
