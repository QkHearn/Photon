#!/bin/bash

echo "=== ArkTS Integration Test ==="
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
echo "  - main.ets (Component with state and UI)"
echo "  - utils.ets (Utility functions and classes)"

# 运行photon符号扫描
echo ""
echo "=== Running Photon Symbol Scan ==="

# 使用photon扫描当前目录
 timeout 30s ../build/photon . << 'EOF'
exit
EOF

echo ""
echo "=== Checking Results ==="

# 检查是否生成了符号索引
if [ -d ".photon" ] && [ -f ".photon/index/symbols.json" ]; then
    echo "✓ Symbol index created"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    
    # 显示符号统计
    if command -v jq >/dev/null 2>&1; then
        echo ""
        echo "Symbol statistics:"
        jq '.files | to_entries | length' .photon/index/symbols.json
        
        echo ""
        echo "Files with symbols:"
        jq -r '.files | keys[]' .photon/index/symbols.json
        
        echo ""
        echo "Sample symbols from main.ets:"
        jq -r '.files["main.ets"].symbols[]? | "  - \(.name) (\(.type)) at line \(.line)"' .photon/index/symbols.json | head -10
    else
        echo "ℹ️  Install jq for detailed JSON analysis"
        echo "Index file contains symbol data"
    fi
else
    echo "⚠️  No symbol index found"
fi

# 清理
cd ..
rm -rf arkts_project

echo ""
echo "=== Test Complete ==="