#!/bin/bash

echo "=== Manual ArkTS Integration Test ==="
echo "Testing Photon with ArkTS files..."

# 创建测试目录
mkdir -p arkts_project
cd arkts_project

# 创建测试ArKTS文件
cat > main.ets << 'EOF'
@Component
struct HelloWorld {
  @State message: string = 'Hello, ArkTS!'
  @State count: number = 0
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
      
      Text(`Count: ${this.count}`)
        .fontSize(18)
      
      Button('Increment')
        .onClick(() => {
          this.count++
        })
        .margin(10)
      
      Button('Reset')
        .onClick(() => {
          this.count = 0
        })
        .margin(10)
    }
    .padding(20)
  }
}

function helperFunction(): string {
  return 'Helper'
}

class UtilityClass {
  static processData(data: string): string {
    return data.toUpperCase()
  }
}
EOF

cat > utils.ets << 'EOF'
export function formatMessage(msg: string): string {
  return `[${new Date().toLocaleTimeString()}] ${msg}`
}

export class Logger {
  private logs: string[] = []
  
  log(message: string): void {
    this.logs.push(formatMessage(message))
  }
  
  getLogs(): string[] {
    return this.logs
  }
}
EOF

echo "✓ Created test ArkTS files"
ls -la *.ets

# 运行photon符号扫描
echo ""
echo "=== Running Photon Symbol Scan ==="
echo "Starting photon (this may take a moment)..."

# 启动photon并等待初始化
../build/photon . &
PHOTON_PID=$!

# 等待photon启动和扫描完成
sleep 10

# 发送退出命令
if kill -0 $PHOTON_PID 2>/dev/null; then
    echo "exit" > /dev/null
    sleep 2
    kill $PHOTON_PID 2>/dev/null || true
fi

echo ""
echo "=== Checking Results ==="

# 检查是否生成了符号索引
if [ -d ".photon" ] && [ -f ".photon/index/symbols.json" ]; then
    echo "✓ Symbol index created"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    echo ""
    echo "Index file content preview:"
    head -20 .photon/index/symbols.json
    
    # 基本统计
    if command -v grep >/dev/null 2>&1; then
        echo ""
        echo "Basic analysis:"
        echo "- Total lines in index: $(wc -l < .photon/index/symbols.json)"
        echo "- 'main.ets' mentions: $(grep -c "main.ets" .photon/index/symbols.json || echo 0)"
        echo "- 'utils.ets' mentions: $(grep -c "utils.ets" .photon/index/symbols.json || echo 0)"
        echo "- Symbol entries: $(grep -c '"name"' .photon/index/symbols.json || echo 0)"
    fi
else
    echo "⚠️  No symbol index found"
    echo "Checking for any photon output..."
    ls -la .photon/ 2>/dev/null || echo "No .photon directory"
fi

# 列出目录内容查看是否有其他输出
echo ""
echo "Directory contents:"
ls -la

# 清理
cd ..
echo ""
echo "Cleaning up test directory..."
rm -rf arkts_project

echo ""
echo "=== Manual Test Complete ==="