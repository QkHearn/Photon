#!/bin/bash

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}开始构建 Photon (不使用 CMake)...${NC}"

# 设置编译器
CXX=g++
if [[ "$OSTYPE" == "darwin"* ]]; then
    CXX=clang++
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

# 源文件列表
SRCS="src/main.cpp src/FileManager.cpp src/LLMClient.cpp src/ContextManager.cpp src/MCPClient.cpp"

# 执行编译
echo "正在执行: $CXX $SRCS $CXXFLAGS $LIBS -o photon"
$CXX $SRCS $CXXFLAGS $LIBS -o photon

if [ $? -eq 0 ]; then
    echo -e "${GREEN}构建成功! 可执行文件: ./photon${NC}"
else
    echo -e "${RED}构建失败，请检查是否安装了依赖库 (openssl, nlohmann-json)${NC}"
    exit 1
fi
