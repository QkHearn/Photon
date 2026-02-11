// 测试ArkTS符号提取功能
@Component
struct MyComponent {
  @State message: string = 'Hello, ArkTS!'
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
      
      Button('Click Me')
        .onClick(() => {
          this.message = 'Button clicked!'
        })
    }
  }
}

// 测试函数定义
function calculateSum(a: number, b: number): number {
  return a + b;
}

// 测试类定义
class Calculator {
  private value: number = 0;
  
  add(num: number): void {
    this.value += num;
  }
  
  getResult(): number {
    return this.value;
  }
}

// 测试接口定义
interface ICalculator {
  add(num: number): void;
  getResult(): number;
}