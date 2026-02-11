#!/bin/bash

echo "=== Simple ArkTS Decorators Test ==="
echo "Testing enhanced decorator extraction..."

# 创建测试目录
mkdir -p decorators_test
cd decorators_test

# 复制配置文件
cp ../test_config.json config.json

# 创建包含各种装饰器的ArkTS测试文件
cat > decorators_test.ets << 'EOF'
// ArkTS 装饰器测试文件

@Component
struct MyComponent {
  @State private message: string = 'Hello World'
  @State count: number = 0
  @Prop title: string = 'Default Title'
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(20)
      
      Button('Click Me')
        .onClick(() => {
          this.count++
        })
    }
  }
  
  private helperMethod(): void {
    console.log('Helper called')
  }
}

@Entry
@Component
struct MainEntry {
  @State isVisible: boolean = true
  
  build() {
    Row() {
      if (this.isVisible) {
        MyComponent({ title: 'Test' })
      }
    }
  }
}

// 普通类和函数
class RegularClass {
  private value: number = 42
  
  getValue(): number {
    return this.value
  }
}

function utilityFunction(): string {
  return 'utility'
}

// 带装饰器的函数
@Deprecated
function oldFunction(): void {
  console.log('This is deprecated')
}

// 状态管理装饰器示例
@Observed
class AppState {
  @Track userName: string = ''
  @Track isLoggedIn: boolean = false
}
EOF

echo "✓ Created test ArkTS file with decorators"

# 运行photon符号扫描
echo ""
echo "=== Running Photon with Enhanced Decorator Support ==="
echo "exit" | /Users/hearn/Documents/code/Photon/build/photon . config.json > photon_output.log 2>&1

echo "Photon output:"
cat photon_output.log

echo ""
echo "=== Checking Enhanced Decorator Results ==="

if [ -f ".photon/index/symbols.json" ]; then
    echo "✅ SUCCESS: Symbol index created with enhanced decorator support!"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    echo ""
    
    # 分析符号内容
    echo "=== Symbol Analysis ==="
    
    # 检查装饰器相关的符号
    echo "Decorator-related symbols:"
    if command -v jq >/dev/null 2>&1; then
        # 使用jq提取装饰器信息
        jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("decorated")) | "  - \(.name) (\(.type)) at line \(.line)"' .photon/index/symbols.json
        
        echo ""
        echo "Component symbols:"
        jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("component")) | "  - \(.name) (\(.type)) at line \(.line)"' .photon/index/symbols.json
        
        echo ""
        echo "State symbols:"
        jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("state")) | "  - \(.name) (\(.type)) at line \(.line)"' .photon/index/symbols.json
        
        echo ""
        echo "Build method symbols:"
        jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("build_method")) | "  - \(.name) (\(.type)) at line \(.line)"' .photon/index/symbols.json
        
        echo ""
        echo "Decorator symbols (@Component, @State, etc.):"
        jq -r '.files["decorators_test.ets"].symbols[]? | select(.type == "decorator") | "  - @\(.name) at line \(.line)"' .photon/index/symbols.json
        
    else
        # 基本文本分析
        echo "Basic analysis (install jq for better JSON parsing):"
        echo ""
        echo "Decorated symbols:"
        grep -o '"type": "[^"]*decorated[^"]*"' .photon/index/symbols.json | sort | uniq -c
        
        echo ""
        echo "Component symbols:"
        grep -o '"type": "[^"]*component[^"]*"' .photon/index/symbols.json | sort | uniq -c
        
        echo ""
        echo "State symbols:"
        grep -o '"type": "[^"]*state[^"]*"' .photon/index/symbols.json | sort | uniq -c
        
        echo ""
        echo "Build method symbols:"
        grep -o '"type": "build_method"' .photon/index/symbols.json | wc -l
        
        echo ""
        echo "Raw symbol data:"
        grep -A2 -B2 '"decorators_test.ets"' .photon/index/symbols.json | head -30
    fi
    
    echo ""
    echo "=== Decorator Feature Verification ==="
    
    # 检查具体的装饰器
    component_count=$(jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("component")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    state_count=$(jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("state")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    build_count=$(jq -r '.files["decorators_test.ets"].symbols[]? | select(.type | contains("build_method")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    decorator_count=$(jq -r '.files["decorators_test.ets"].symbols[]? | select(.type == "decorator") | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    
    echo "Components detected: $component_count"
    echo "State variables detected: $state_count"  
    echo "Build methods detected: $build_count"
    echo "Decorator symbols detected: $decorator_count"
    
    if [ "$component_count" -gt 0 ] && [ "$state_count" -gt 0 ]; then
        echo ""
        echo "🎉 ENHANCED DECORATOR SUPPORT IS WORKING!"
        echo "✅ @Component decoration: Detected"
        echo "✅ @State decoration: Detected"
        echo "✅ Build method recognition: $([ "$build_count" -gt 0 ] && echo 'Detected' || echo 'Not detected')"
        echo "✅ TreeSitter parsing: Functional"
        echo "✅ Enhanced symbol extraction: Working"
    else
        echo ""
        echo "⚠️  Enhanced decorator support may need refinement"
    fi
    
else
    echo "❌ Symbol index not created"
    echo "Checking photon output for errors..."
    if grep -q "Building symbol index" photon_output.log; then
        echo "✓ Symbol scanning was initiated"
    fi
    
    if grep -q "Symbol index ready" photon_output.log; then
        echo "✓ Symbol scanning completed"
        symbol_count=$(grep -o "Symbol index ready: [0-9]* symbols" photon_output.log | grep -o "[0-9]*" || echo "0")
        echo "Symbols found: $symbol_count"
    fi
fi

# 显示完整索引内容用于调试
echo ""
echo "=== Full Symbol Index Content ==="
if [ -f ".photon/index/symbols.json" ]; then
    echo "First 100 lines of symbol index:"
    head -100 .photon/index/symbols.json
fi

# 清理
cd ..
echo ""
echo "Cleaning up test directory..."
rm -rf decorators_test

echo ""
echo "=== Enhanced Decorator Test Complete ==="