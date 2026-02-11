// TypeScript 测试文件 - 包含各种TypeScript特性

// 接口定义
interface Person {
    name: string;
    age: number;
    email?: string;
}

interface Vehicle {
    brand: string;
    model: string;
    start(): string;
    stop(): string;
}

// 类型别名
type Status = 'pending' | 'active' | 'completed';
type ID = string | number;
type Callback<T> = (data: T) => void;

// 枚举
enum Color {
    Red = 'RED',
    Green = 'GREEN',
    Blue = 'BLUE'
}

enum HttpStatus {
    OK = 200,
    NotFound = 404,
    InternalServerError = 500
}

// 类定义
class Animal {
    protected name: string;
    
    constructor(name: string) {
        this.name = name;
    }
    
    speak(): string {
        return `${this.name} makes a sound`;
    }
}

class Dog extends Animal {
    private breed: string;
    
    constructor(name: string, breed: string) {
        super(name);
        this.breed = breed;
    }
    
    bark(): string {
        return `${this.name} the ${this.breed} barks!`;
    }
    
    // 静态方法
    static createPuppy(name: string): Dog {
        return new Dog(name, 'Puppy');
    }
}

// 抽象类
abstract class AbstractShape {
    abstract getArea(): number;
    abstract getPerimeter(): number;
    
    describe(): string {
        return `Shape with area ${this.getArea()}`;
    }
}

class Rectangle extends AbstractShape {
    constructor(private width: number, private height: number) {
        super();
    }
    
    getArea(): number {
        return this.width * this.height;
    }
    
    getPerimeter(): number {
        return 2 * (this.width + this.height);
    }
}

// 泛型类
class Stack<T> {
    private items: T[] = [];
    
    push(item: T): void {
        this.items.push(item);
    }
    
    pop(): T | undefined {
        return this.items.pop();
    }
    
    peek(): T | undefined {
        return this.items[this.items.length - 1];
    }
    
    isEmpty(): boolean {
        return this.items.length === 0;
    }
}

// 泛型函数
function identity<T>(arg: T): T {
    return arg;
}

function mapArray<T, U>(array: T[], fn: (item: T) => U): U[] {
    return array.map(fn);
}

// 箭头函数
const add = (a: number, b: number): number => a + b;
const multiply = (x: number, y: number): number => x * y;

// 函数重载（通过联合类型实现）
function processValue(value: string): string;
function processValue(value: number): number;
function processValue(value: string | number): string | number {
    if (typeof value === 'string') {
        return value.toUpperCase();
    } else {
        return value * 2;
    }
}

// 可选参数和默认参数
function greet(name: string, greeting?: string): string {
    const actualGreeting = greeting || 'Hello';
    return `${actualGreeting}, ${name}!`;
}

function createUser(name: string, age: number = 18): Person {
    return { name, age };
}

// 装饰器函数（TypeScript装饰器）
function log(target: any, propertyKey: string, descriptor: PropertyDescriptor) {
    const originalMethod = descriptor.value;
    descriptor.value = function (...args: any[]) {
        console.log(`Calling ${propertyKey} with args:`, args);
        const result = originalMethod.apply(this, args);
        console.log(`Method ${propertyKey} returned:`, result);
        return result;
    };
}

function measure(target: any, propertyKey: string, descriptor: PropertyDescriptor) {
    const originalMethod = descriptor.value;
    descriptor.value = function (...args: any[]) {
        const start = performance.now();
        const result = originalMethod.apply(this, args);
        const end = performance.now();
        console.log(`Method ${propertyKey} took ${end - start} milliseconds`);
        return result;
    };
}

// 使用装饰器的类
class Calculator {
    @log
    @measure
    add(a: number, b: number): number {
        return a + b;
    }
    
    multiply(a: number, b: number): number {
        return a * b;
    }
}

// 异步函数
async function fetchData(url: string): Promise<any> {
    // 模拟异步操作
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve({ url, data: 'sample data' });
        }, 1000);
    });
}

async function processAsync(): Promise<void> {
    try {
        const result = await fetchData('https://api.example.com/data');
        console.log('Async result:', result);
    } catch (error) {
        console.error('Async error:', error);
    }
}

// 高级类型
type User = {
    id: number;
    name: string;
    email: string;
    roles: string[];
};

type AdminUser = User & {
    permissions: string[];
    isActive: boolean;
};

type Nullable<T> = T | null;
type Optional<T> = T | undefined;

// 条件类型
type IsString<T> = T extends string ? true : false;
type ExtractArrayType<T> = T extends (infer U)[] ? U : never;

// 映射类型
type Readonly<T> = {
    readonly [P in keyof T]: T[P];
};

type Partial<T> = {
    [P in keyof T]?: T[P];
};

// 命名空间
namespace Utils {
    export function formatDate(date: Date): string {
        return date.toISOString().split('T')[0];
    }
    
    export class StringUtils {
        static capitalize(str: string): string {
            return str.charAt(0).toUpperCase() + str.slice(1);
        }
        
        static reverse(str: string): string {
            return str.split('').reverse().join('');
        }
    }
}

// 模块导出
export interface ILogger {
    log(message: string): void;
    error(message: string): void;
}

export class ConsoleLogger implements ILogger {
    log(message: string): void {
        console.log(`[LOG] ${message}`);
    }
    
    error(message: string): void {
        console.error(`[ERROR] ${message}`);
    }
}

// 混合类型
interface Timestamped {
    createdAt: Date;
    updatedAt: Date;
}

interface Identifiable {
    id: string;
}

type TimestampedEntity = Timestamped & Identifiable;

// 实用工具类型
type KeysOfType<T, U> = {
    [K in keyof T]: T[K] extends U ? K : never;
}[keyof T];

type PickByType<T, U> = Pick<T, KeysOfType<T, U>>;

// 主函数
function main(): void {
    console.log("TypeScript Multilingual Test");
    
    // 基本函数测试
    console.log(greet("World"));
    console.log(add(5, 3));
    console.log(multiply(4, 7));
    
    // 类测试
    const dog = new Dog("Buddy", "Golden Retriever");
    console.log(dog.bark());
    console.log(dog.speak());
    
    const puppy = Dog.createPuppy("Max");
    console.log(puppy.bark());
    
    // 泛型测试
    const numberStack = new Stack<number>();
    numberStack.push(1);
    numberStack.push(2);
    console.log(`Stack peek: ${numberStack.peek()}`);
    
    const stringStack = new Stack<string>();
    stringStack.push("hello");
    stringStack.push("world");
    console.log(`Stack peek: ${stringStack.peek()}`);
    
    // 枚举测试
    console.log(`Color: ${Color.Red}`);
    console.log(`HTTP Status: ${HttpStatus.OK}`);
    
    // 函数重载测试
    console.log(`Process string: ${processValue("hello")}`);
    console.log(`Process number: ${processValue(42)}`);
    
    // 可选参数和默认参数测试
    console.log(greet("Alice"));
    console.log(greet("Bob", "Hi"));
    
    const user = createUser("Charlie", 25);
    console.log(`User: ${JSON.stringify(user)}`);
    
    // 装饰器测试
    const calc = new Calculator();
    console.log(`Add result: ${calc.add(10, 20)}`);
    console.log(`Multiply result: ${calc.multiply(5, 6)}`);
    
    // 命名空间测试
    console.log(`Formatted date: ${Utils.formatDate(new Date())}`);
    console.log(`Capitalized: ${Utils.StringUtils.capitalize("hello")}`);
    console.log(`Reversed: ${Utils.StringUtils.reverse("world")}`);
    
    // 高级类型测试
    const adminUser: AdminUser = {
        id: 1,
        name: "Admin",
        email: "admin@example.com",
        roles: ["admin", "user"],
        permissions: ["read", "write", "delete"],
        isActive: true
    };
    
    console.log(`Admin user: ${JSON.stringify(adminUser)}`);
    
    // 异步测试（注释掉以避免运行时依赖）
    // processAsync().then(() => console.log("Async processing completed"));
}

// 导出函数和类
export { main, Person, Vehicle, Calculator, Stack, Color, Status };
export default main;