#!/bin/bash

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}开始本地代码覆盖率测试...${NC}"

# 1. 检查 lcov 是否安装
if ! command -v lcov >/dev/null 2>&1; then
    echo -e "${RED}错误: 未找到 lcov。请运行 'brew install lcov' (macOS) 或 'sudo apt install lcov' (Linux)${NC}"
    exit 1
fi

# 2. 清理并创建构建目录
rm -rf build-coverage
mkdir -p build-coverage
cd build-coverage || exit 1

# 3. 使用覆盖率标志进行编译
echo -e "${GREEN}正在配置 CMake (Debug + Coverage)...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DPHOTON_ENABLE_TREESITTER=OFF

echo -e "${GREEN}正在构建...${NC}"
cmake --build . --config Debug

# 4. 运行测试以生成数据
echo -e "${GREEN}正在运行单元测试...${NC}"
ctest --output-on-failure

# 5. 收集覆盖率数据
echo -e "${GREEN}正在收集覆盖率数据...${NC}"
lcov --capture --directory . --output-file coverage.info \
    --ignore-errors mismatch,inconsistent,unsupported,format,unused \
    --exclude '/usr/*' \
    --exclude '*/third_party/*' \
    --exclude '*/src/utils/httplib.h' \
    --exclude '*/tests/*' \
    --exclude '*/build*'

# 6. 生成 HTML 报告
echo -e "${GREEN}正在生成 HTML 报告...${NC}"
genhtml coverage.info --output-directory coverage-report \
    --ignore-errors source,format,inconsistent,corrupt,category

echo -e "${GREEN}完成！报告已生成在: build-coverage/coverage-report/index.html${NC}"
if [[ "$OSTYPE" == "darwin"* ]]; then
    open coverage-report/index.html
fi
