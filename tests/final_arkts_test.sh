#!/bin/bash

echo "=== Final ArkTS Integration Test ==="
echo "Testing Photon with ArkTS files..."

# 创建测试目录
mkdir -p arkts_project
cd arkts_project

# 创建测试ArkTS文件
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
echo "Starting photon with direct exit..."

# 使用echo管道启动photon并立即退出
echo "exit" | /Users/hearn/Documents/code/Photon/build/photon . 2>&1 | head -30

echo ""
echo "=== Checking Results ==="

# 检查是否生成了符号索引
if [ -d ".photon" ] && [ -f ".photon/index/symbols.json" ]; then
    echo "✓ Symbol index created successfully!"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    echo ""
    echo "First 30 lines of index:"
    head -30 .photon/index/symbols.json
    
    # 基本统计
    echo ""
    echo "Symbol analysis:"
    echo "- Total symbol entries: $(grep -c '"name"' .photon/index/symbols.json 2>/dev/null || echo 0)"
    echo "- Files processed: $(grep -c '"files"' .photon/index/symbols.json 2>/dev/null || echo 0)"
    
    # 检查具体文件
    if grep -q "main.ets" .photon/index/symbols.json; then
        echo "✓ main.ets was processed"
    fi
    if grep -q "utils.ets" .photon/index/symbols.json; then
        echo "✓ utils.ets was processed"
    fi
    
else
    echo "⚠️  No symbol index found"
    echo "Checking for .photon directory:"
    ls -la .photon/ 2>/dev/null || echo "No .photon directory created"
fi

# 显示目录内容
echo ""
echo "Final directory contents:"
ls -la

# 清理
cd ..
echo ""
echo "Cleaning up test directory..."
rm -rf arkts_project

echo ""
echo "=== Final Test Complete ==="
echo "Test results indicate ArkTS support status above."