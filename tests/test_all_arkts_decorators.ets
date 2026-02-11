// 完整的ArkTS装饰器测试文件 - 包含所有类型的装饰器

// 1. 组件与入口装饰器
@Entry
@Component
struct MainEntry {
  @State message: string = 'Hello ArkTS!'
  @Prop title: string = 'Main Page'
  
  build() {
    Column() {
      Text(this.message)
        .fontSize(24)
      
      MyComponent({ title: this.title })
    }
  }
}

@Component
struct MyComponent {
  @Link count: number
  @Prop title: string
  @State private isVisible: boolean = true
  
  build() {
    Column() {
      if (this.isVisible) {
        Text(`Count: ${this.count}`)
          .fontSize(18)
        
        Button('Increment')
          .onClick(() => {
            this.count++
          })
      }
    }
  }
}

// 2. 状态管理装饰器示例
@Observed
class AppState {
  @Track userName: string = ''
  @Track isLoggedIn: boolean = false
  
  login(name: string): void {
    this.userName = name
    this.isLoggedIn = true
  }
}

@ObservedV2
class ModernState {
  @Trace activeCount: number = 0
}

// 3. UI构建装饰器
@Builder
function CustomButton(label: string, onClick: () => void): void {
  Button(label)
    .onClick(onClick)
    .backgroundColor('#007AFF')
    .borderRadius(8)
}

@LocalBuilder
function LocalButton(text: string): void {
  Button(text)
    .margin(5)
}

// 4. 样式扩展装饰器
@Extend(Button)
function fancyButton(fontSize: number) {
  .fontSize(fontSize)
  .backgroundColor(Color.Blue)
  .borderRadius(10)
}

@Styles
function commonStyles() {
  .width('100%')
  .padding(10)
  .margin(5)
}

@AnimatableExtend(Text)
function animatableText(size: number) {
  .fontSize(size)
  .animation({ duration: 300 })
}

// 5. 存储装饰器
@Component
struct StorageExample {
  @LocalStorageLink('userName') userName: string = ''
  @LocalStorageProp('theme') theme: string = 'light'
  @StorageLink('settings') settings: object = {}
  @StorageProp('version') version: string = '1.0.0'
  
  build() {
    Column() {
      Text(`User: ${this.userName}`)
      Text(`Theme: ${this.theme}`)
    }
  }
}

// 6. 监控和计算装饰器
@Component
struct MonitorExample {
  @Monitor('count')
  onCountChanged(count: number): void {
    console.log(`Count changed to: ${count}`)
  }
  
  @Computed
  get doubledCount(): number {
    return this.count * 2
  }
  
  @State count: number = 0
  
  build() {
    Column() {
      Text(`Count: ${this.count}`)
      Text(`Doubled: ${this.doubledCount}`)
    }
  }
}

// 7. 参数和事件装饰器
@Component
struct ParamExample {
  @Param message: string = 'Default'
  @Once onInit(): void {
    console.log('Component initialized once')
  }
  @Event onCustomEvent: (data: string) => void = (data) => {}
  
  build() {
    Text(this.message)
  }
}

// 8. 提供者消费者装饰器
@Component
struct ProviderExample {
  @Provide theme: string = 'light'
  @Provide userInfo: object = { name: 'User' }
  
  build() {
    ConsumerComponent()
  }
}

@Component
struct ConsumerExample {
  @Consume theme: string
  @Consume userInfo: object
  
  build() {
    Column() {
      Text(`Theme: ${this.theme}`)
      Text(`User: ${this.userInfo.name}`)
    }
  }
}

// 9. 要求装饰器
@Component
struct RequireExample {
  @Require name: string
  @Require age: number
  
  build() {
    Text(`${this.name} is ${this.age} years old`)
  }
}

// 10. 类型和跟踪装饰器
@Component
struct TypeExample {
  @Type('string') id: any = '123'
  @Trace methodCalls: number = 0
  
  private someMethod(): void {
    this.methodCalls++
  }
  
  build() {
    Text('Type Example')
  }
}

// 11. 通用TypeScript装饰器兼容性
@Deprecated
function legacyFunction(): string {
  return 'This is deprecated'
}

@Component
struct DeprecatedExample {
  @Deprecated
  oldMethod(): void {
    console.log('Old method')
  }
  
  build() {
    Text('Deprecated example')
  }
}

// 12. 自定义状态样式
@Component
struct StateStylesExample {
  @State isPressed: boolean = false
  @State isDisabled: boolean = false
  
  build() {
    Button('Click me')
      .stateStyles({
        pressed: {
          .backgroundColor(Color.Red)
          .scale({ x: 0.95, y: 0.95 })
        },
        normal: {
          .backgroundColor(Color.Blue)
        },
        disabled: {
          .backgroundColor(Color.Gray)
          .opacity(0.6)
        }
      })
      .onClick(() => {
        this.isPressed = !this.isPressed
      })
  }
}

// 13. 对象链接装饰器
@Component
struct ObjectLinkExample {
  @ObjectLink user: User
  
  build() {
    Column() {
      Text(this.user.name)
      Text(this.user.email)
    }
  }
}

@Observed
class User {
  name: string = ''
  email: string = ''
}

// 14. 条件渲染装饰器
@Component
struct ConditionalExample {
  @LocalStorageProp('isDarkMode') isDarkMode: boolean = false
  @StorageProp('language') language: string = 'en'
  
  build() {
    Column() {
      if (this.isDarkMode) {
        Text('Dark Mode')
      } else {
        Text('Light Mode')
      }
      
      Text(`Language: ${this.language}`)
    }
  }
}

// 15. 高级构建器参数
@Component
struct AdvancedBuilderExample {
  @BuilderParam headerBuilder: () => void
  @BuilderParam footerBuilder: () => void = this.defaultFooter
  
  @Builder
  defaultFooter(): void {
    Text('Default Footer')
  }
  
  build() {
    Column() {
      this.headerBuilder()
      Text('Main Content')
      this.footerBuilder()
    }
  }
}