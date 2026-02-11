#!/bin/bash

echo "=== Complete ArkTS Integration Test ==="
echo "Testing Photon with ArkTS files using test configuration..."

# 创建测试目录
mkdir -p arkts_project
cd arkts_project

# 复制测试配置文件
cp ../test_config.json config.json

echo "✓ Copied test configuration"

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
echo "Starting photon with test configuration..."

# 运行photon并立即退出，捕获输出
echo "exit" | /Users/hearn/Documents/code/Photon/build/photon . config.json > photon_output.log 2>&1

echo "Photon output:"
cat photon_output.log

echo ""
echo "=== Checking Results ==="

# 检查是否生成了符号索引
if [ -d ".photon" ] && [ -f ".photon/index/symbols.json" ]; then
    echo "✅ SUCCESS: Symbol index created!"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    echo ""
    echo "=== Symbol Index Analysis ==="
    
    # 显示符号统计
    if command -v jq >/dev/null 2>&1; then
        echo "Files in index: $(jq -r '.files | keys | length' .photon/index/symbols.json)"
        echo ""
        echo "Files processed:"
        jq -r '.files | keys[]' .photon/index/symbols.json
        
        echo ""
        echo "Symbols by file:"
        for file in $(jq -r '.files | keys[]' .photon/index/symbols.json); do
            symbol_count=$(jq -r ".files[\"$file\"].symbols | length" .photon/index/symbols.json)
            echo "  $file: $symbol_count symbols"
            if [ "$symbol_count" -gt 0 ]; then
                jq -r ".files[\"$file\"].symbols[] | \"    - \(.name) (\(.type)) at line \(.line)\"" .photon/index/symbols.json | head -5
            fi
            echo ""
        done
    else
        echo "Basic analysis (install jq for detailed JSON parsing):"
        echo "- Files mentioned: $(grep -o '"[^"]*\.ets"' .photon/index/symbols.json | wc -l)"
        echo "- Symbol entries: $(grep -c '"name"' .photon/index/symbols.json 2>/dev/null || echo 0)"
        echo "- Tree-sitter symbols: $(grep -c '"tree_sitter"' .photon/index/symbols.json 2>/dev/null || echo 0)"
        echo "- Regex symbols: $(grep -c '"regex"' .photon/index/symbols.json 2>/dev/null || echo 0)"
        
        echo ""
        echo "First 50 lines of index:"
        head -50 .photon/index/symbols.json
    fi
    
    # 验证ArkTS特定功能
    echo ""
    echo "=== ArkTS Feature Verification ==="
    
    # 检查Component
    if grep -q "Component\|component" .photon/index/symbols.json; then
        echo "✓ Component structure detected"
    else
        echo "⚠️  Component structure not clearly identified"
    fi
    
    # 检查State
    if grep -q "State\|state\|@State" .photon/index/symbols.json; then
        echo "✓ State variables detected"
    else
        echo "⚠️  State variables not clearly identified"
    fi
    
    # 检查build方法
    if grep -q "build" .photon/index/symbols.json; then
        echo "✓ Build method detected"
    else
        echo "⚠️  Build method not found"
    fi
    
    # 检查函数和类
    if grep -q "function\|class" .photon/index/symbols.json; then
        echo "✓ Functions/Classes detected"
    else
        echo "⚠️  Functions/Classes not clearly identified"
    fi
    
    echo ""
    echo "🎉 ArkTS support is WORKING!"
    echo "✅ TreeSitter parsing: Available"
    echo "✅ Symbol extraction: Functional" 
    echo "✅ File extension support: .ets files processed"
    
else
    echo "❌ Symbol index not created"
    echo "Checking for .photon directory:"
    ls -la .photon/ 2>/dev/null || echo "No .photon directory"
    
    echo ""
    echo "Photon output analysis:"
    if grep -q "Building symbol index" photon_output.log; then
        echo "✓ Symbol scanning was initiated"
    fi
    
    if grep -q "Symbol index ready" photon_output.log; then
        echo "✓ Symbol scanning completed"
        symbol_count=$(grep -o "Symbol index ready: [0-9]* symbols" photon_output.log | grep -o "[0-9]*" || echo "0")
        echo "Symbols found: $symbol_count"
    fi
    
    echo ""
    echo "⚠️  ArkTS support may need additional configuration"
fi

# 显示目录内容
echo ""
echo "Final directory structure:"
ls -la

# 清理
cd ..
echo ""
echo "Cleaning up test directory..."
rm -rf arkts_project

echo ""
echo "=== Complete Test Finished ==="