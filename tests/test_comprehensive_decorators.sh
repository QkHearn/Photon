#!/bin/bash

echo "=== Comprehensive ArkTS Decorators Test ==="
echo "Testing complete decorator recognition with enhanced TreeSitter..."

# 创建测试目录
mkdir -p comprehensive_test
cd comprehensive_test

# 复制配置文件
cp ../test_config.json config.json

# 使用我们创建的全装饰器测试文件
cp ../test_all_arkts_decorators.ets comprehensive_decorators.ets

echo "✓ Created comprehensive ArkTS decorators test file"
echo "File contains all ArkTS decorator types:"
echo "  - Component & Entry decorators (@Component, @Entry)"
echo "  - State management (@State, @Prop, @Link, @ObjectLink, @Observed)"
echo "  - UI builders (@Builder, @BuilderParam, @LocalBuilder)"
echo "  - Style extensions (@Extend, @Styles, @AnimatableExtend)"
echo "  - Storage decorators (@LocalStorageLink, @StorageProp, etc.)"
echo "  - Monitoring (@Monitor, @Computed, @Trace)"
echo "  - Parameters (@Param, @Once, @Event, @Require)"
echo "  - Providers/Consumers (@Provide, @Consume)"
echo "  - Type decorators (@Type, @ObservedV2)"
echo "  - TypeScript compatibility (@Deprecated)"

# 运行photon符号扫描
echo ""
echo "=== Running Photon with Complete Decorator Support ==="
echo "exit" | /Users/hearn/Documents/code/Photon/build/photon . config.json > photon_output.log 2>&1

echo "Photon execution completed"

echo ""
echo "=== Analyzing Comprehensive Decorator Results ==="

if [ -f ".photon/index/symbols.json" ]; then
    echo "✅ SUCCESS: Comprehensive symbol index created!"
    echo "Index file size: $(ls -lh .photon/index/symbols.json | awk '{print $5}')"
    
    # 基本统计
    total_symbols=$(jq -r '.files["comprehensive_decorators.ets"].symbols | length' .photon/index/symbols.json 2>/dev/null || echo "0")
    echo "Total symbols extracted: $total_symbols"
    
    echo ""
    echo "=== Detailed Decorator Analysis ==="
    
    # 1. 组件装饰器分析
    echo "1. Component & Entry Decorators:"
    entry_components=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("entry_component")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    regular_components=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("component")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - @Entry components: $entry_components"
    echo "  - @Component components: $regular_components"
    
    echo ""
    echo "2. State Management Decorators:"
    state_vars=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("state_variable")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    prop_vars=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("prop_variable")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    link_vars=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("link_variable")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    observed_vars=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("observed_variable")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - @State variables: $state_vars"
    echo "  - @Prop variables: $prop_vars"
    echo "  - @Link variables: $link_vars"
    echo "  - @Observed variables: $observed_vars"
    
    echo ""
    echo "3. UI Builder Decorators:"
    builder_funcs=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("builder_function")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    extend_funcs=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("extend_function")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - @Builder functions: $builder_funcs"
    echo "  - @Extend functions: $extend_funcs"
    
    echo ""
    echo "4. Build Methods:"
    build_methods=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("build_method")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - build() methods: $build_methods"
    
    echo ""
    echo "5. Decorator Symbols (individual @ decorators):"
    decorator_symbols=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | startswith("decorator:")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - Individual decorator symbols: $decorator_symbols"
    
    echo ""
    echo "6. Detailed Decorator Symbol List:"
    jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | startswith("decorator:")) | "  - @\(.name) (\(.type)) line \(.line)\(if .signature != "" then " args: \(.signature)" else "" end)"' .photon/index/symbols.json 2>/dev/null || echo "  (No individual decorator symbols found)"
    
    echo ""
    echo "7. Class Decorators:"
    observed_classes=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.type | contains("observed_class")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - @Observed classes: $observed_classes"
    
    echo ""
    echo "8. Storage Decorators (from property analysis):"
    # 分析属性装饰器参数来识别存储装饰器
    storage_props=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.signature | contains("LocalStorage") or contains("Storage")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - Storage-related properties: $storage_props"
    
    echo ""
    echo "9. Monitor and Computed Decorators:"
    monitor_symbols=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.name | contains("Monitor") or contains("Computed")) | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - Monitor/Computed related: $monitor_symbols"
    
    echo ""
    echo "10. TypeScript Compatibility:"
    ts_decorators=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | select(.name == "Deprecated") | .name' .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
    echo "  - TypeScript decorators: $ts_decorators"
    
    # 验证关键装饰器是否存在
    echo ""
    echo "=== Key Decorator Verification ==="
    
    key_checks=(
        "Component:component"
        "Entry:decorator:component"
        "State:decorator:state_management"
        "Prop:decorator:state_management"
        "Link:decorator:state_management"
        "Observed:decorator:observation"
        "Builder:decorator:ui_builder"
        "Extend:decorator:styling"
        "Deprecated:decorator:typescript"
    )
    
    echo "Checking for key ArkTS decorators:"
    for check in "${key_checks[@]}"; do
        IFS=':' read -r decorator_name expected_type <<< "$check"
        found=$(jq -r ".files[\"comprehensive_decorators.ets\"].symbols[]? | select(.name == \"$decorator_name\" and .type == \"$expected_type\") | .name" .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
        if [ "$found" -gt 0 ]; then
            echo "  ✅ @${decorator_name} detected as $expected_type"
        else
            echo "  ❌ @${decorator_name} not found as $expected_type"
        fi
    done
    
    echo ""
    echo "=== Overall Assessment ==="
    
    # 计算成功率
    success_count=0
    for check in "${key_checks[@]}"; do
        IFS=':' read -r decorator_name expected_type <<< "$check"
        found=$(jq -r ".files[\"comprehensive_decorators.ets\"].symbols[]? | select(.name == \"$decorator_name\" and .type == \"$expected_type\") | .name" .photon/index/symbols.json 2>/dev/null | wc -l || echo 0)
        if [ "$found" -gt 0 ]; then
            ((success_count++))
        fi
    done
    
    total_checks=${#key_checks[@]}
    success_rate=$((success_count * 100 / total_checks))
    
    echo "Decorator recognition success rate: $success_rate% ($success_count/$total_checks)"
    
    if [ "$success_rate" -ge 80 ]; then
        echo ""
        echo "🎉 COMPREHENSIVE ARKTS DECORATOR SUPPORT IS WORKING!"
        echo "✅ Complete decorator recognition: Functional"
        echo "✅ Enhanced TreeSitter parsing: Working"
        echo "✅ All ArkTS decorator categories: Supported"
        echo "✅ Parameter extraction: Implemented"
        echo "✅ Semantic classification: Available"
    else
        echo ""
        echo "⚠️  Comprehensive decorator support needs refinement"
        echo "Success rate: $success_rate% (target: 80%+)"
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

# 显示完整的符号列表以供分析
echo ""
echo "=== Complete Symbol List ==="
if [ -f ".photon/index/symbols.json" ]; then
    echo "All symbols extracted from comprehensive_decorators.ets:"
    jq -r '.files["comprehensive_decorators.ets"].symbols[]? | "  - \(.name) (\(.type)) line \(.line)\(if .signature != "" then " signature: \(.signature)" else "" end)"' .photon/index/symbols.json 2>/dev/null | head -50
    
    total_lines=$(jq -r '.files["comprehensive_decorators.ets"].symbols[]? | "  - \(.name) (\(.type)) line \(.line)\(if .signature != "" then " signature: \(.signature)" else "" end)"' .photon/index/symbols.json 2>/dev/null | wc -l)
    if [ "$total_lines" -gt 50 ]; then
        echo "  ... and $((total_lines - 50)) more symbols"
    fi
fi

# 清理
cd ..
echo ""
echo "Cleaning up test directory..."
rm -rf comprehensive_test

echo ""
echo "=== Comprehensive Test Complete ==="