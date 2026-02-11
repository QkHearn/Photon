// JavaScript 测试文件 - 包含各种JavaScript特性

// ES6+ 特性

// 类定义（ES6）
class Animal {
    constructor(name) {
        this.name = name;
    }
    
    speak() {
        return `${this.name} makes a sound`;
    }
}

class Dog extends Animal {
    constructor(name, breed) {
        super(name);
        this.breed = breed;
    }
    
    bark() {
        return `${this.name} the ${this.breed} barks!`;
    }
    
    // 静态方法
    static createPuppy(name) {
        return new Dog(name, 'Puppy');
    }
}

// 箭头函数
const add = (a, b) => a + b;
const multiply = (x, y) => x * y;
const greet = name => `Hello, ${name}!`;

// 模板字符串
const createMessage = (name, age) => {
    return `Hello ${name}, you are ${age} years old.`;
};

// 解构赋值
const person = { name: 'Alice', age: 30, city: 'New York' };
const { name, age } = person;

const numbers = [1, 2, 3, 4, 5];
const [first, second, ...rest] = numbers;

// 默认参数
function greetWithDefault(name = 'World', greeting = 'Hello') {
    return `${greeting}, ${name}!`;
}

// 剩余参数
function sum(...args) {
    return args.reduce((total, num) => total + num, 0);
}

// 展开运算符
const arr1 = [1, 2, 3];
const arr2 = [4, 5, 6];
const combined = [...arr1, ...arr2];

const obj1 = { a: 1, b: 2 };
const obj2 = { c: 3, d: 4 };
const merged = { ...obj1, ...obj2 };

// 对象方法简写
const calculator = {
    value: 0,
    add(num) {
        this.value += num;
        return this;
    },
    multiply(num) {
        this.value *= num;
        return this;
    },
    getValue() {
        return this.value;
    }
};

// 计算属性名
const prefix = 'user';
const dynamicObject = {
    [`${prefix}Name`]: 'John',
    [`${prefix}Age`]: 25,
    [`${prefix}Email`]: 'john@example.com'
};

// Symbol
const sym1 = Symbol('description');
const sym2 = Symbol.for('global');

// Map 和 Set
const userMap = new Map();
userMap.set('name', 'Alice');
userMap.set('age', 30);

const uniqueNumbers = new Set([1, 2, 3, 3, 4, 4, 5]);

// Promise
const fetchData = (url) => {
    return new Promise((resolve, reject) => {
        setTimeout(() => {
            resolve({ url, data: 'sample data' });
        }, 1000);
    });
};

// Async/Await
async function processAsync() {
    try {
        const result = await fetchData('https://api.example.com/data');
        console.log('Async result:', result);
    } catch (error) {
        console.error('Async error:', error);
    }
}

// 生成器函数
function* fibonacciGenerator(n) {
    let a = 0, b = 1;
    for (let i = 0; i < n; i++) {
        yield a;
        [a, b] = [b, a + b];
    }
}

// 高阶函数
function createMultiplier(factor) {
    return function(x) {
        return x * factor;
    };
}

const timesFour = createMultiplier(4);

// 数组方法
const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

const doubled = numbers.map(n => n * 2);
const evens = numbers.filter(n => n % 2 === 0);
const sum = numbers.reduce((acc, n) => acc + n, 0);
const hasEven = numbers.some(n => n % 2 === 0);
const allPositive = numbers.every(n => n > 0);

// 对象方法
const person = {
    firstName: 'John',
    lastName: 'Doe',
    get fullName() {
        return `${this.firstName} ${this.lastName}`;
    },
    set fullName(name) {
        [this.firstName, this.lastName] = name.split(' ');
    }
};

// JSON操作
const jsonData = {
    name: 'Product',
    price: 99.99,
    categories: ['electronics', 'gadgets'],
    inStock: true
};

const jsonString = JSON.stringify(jsonData);
const parsedData = JSON.parse(jsonString);

// 正则表达式
const emailRegex = /^[\w-\.]+@([\w-]+\.)+[\w-]{2,4}$/;
const isValidEmail = (email) => emailRegex.test(email);

// 错误处理
function divide(a, b) {
    try {
        if (b === 0) {
            throw new Error('Division by zero');
        }
        return a / b;
    } catch (error) {
        console.error('Error:', error.message);
        return 0;
    } finally {
        console.log('Division operation completed');
    }
}

// 模块模式（CommonJS风格）
const MathModule = (function() {
    const PI = 3.14159;
    
    function circleArea(radius) {
        return PI * radius * radius;
    }
    
    function circleCircumference(radius) {
        return 2 * PI * radius;
    }
    
    return {
        PI,
        circleArea,
        circleCircumference
    };
})();

// 代理（Proxy）
const handler = {
    get(target, prop) {
        console.log(`Getting property: ${prop}`);
        return target[prop];
    },
    set(target, prop, value) {
        console.log(`Setting property: ${prop} = ${value}`);
        target[prop] = value;
        return true;
    }
};

const proxyObject = new Proxy({}, handler);

// 弱引用
const weakMap = new WeakMap();
const weakSet = new WeakSet();

// 主函数
function main() {
    console.log("JavaScript Multilingual Test");
    
    // 类测试
    const dog = new Dog("Buddy", "Golden Retriever");
    console.log(dog.bark());
    console.log(dog.speak());
    
    const puppy = Dog.createPuppy("Max");
    console.log(puppy.bark());
    
    // 箭头函数测试
    console.log(`Add: ${add(5, 3)}`);
    console.log(`Multiply: ${multiply(4, 7)}`);
    console.log(`Greet: ${greet("World")}`);
    
    // 模板字符串测试
    console.log(createMessage("Alice", 30));
    
    // 解构赋值测试
    console.log(`Name: ${name}, Age: ${age}`);
    console.log(`First: ${first}, Second: ${second}, Rest: ${rest}`);
    
    // 默认参数测试
    console.log(greetWithDefault());
    console.log(greetWithDefault("Bob", "Hi"));
    
    // 剩余参数测试
    console.log(`Sum: ${sum(1, 2, 3, 4, 5)}`);
    
    // 展开运算符测试
    console.log(`Combined: ${combined}`);
    console.log(`Merged:`, merged);
    
    // 对象方法测试
    const result = calculator.add(10).multiply(2).getValue();
    console.log(`Calculator result: ${result}`);
    
    // 计算属性名测试
    console.log(`Dynamic object:`, dynamicObject);
    
    // Map和Set测试
    console.log(`User from Map: ${userMap.get('name')}`);
    console.log(`Unique numbers:`, [...uniqueNumbers]);
    
    // Promise测试（注释掉以避免运行时依赖）
    // fetchData('https://api.example.com/data').then(result => {
    //     console.log('Promise result:', result);
    // });
    
    // 生成器测试
    const fibSequence = [...fibonacciGenerator(10)];
    console.log(`Fibonacci sequence: ${fibSequence}`);
    
    // 高阶函数测试
    console.log(`Times four of 5: ${timesFour(5)}`);
    
    // 数组方法测试
    console.log(`Doubled: ${doubled}`);
    console.log(`Evens: ${evens}`);
    console.log(`Sum: ${sum}`);
    console.log(`Has even: ${hasEven}`);
    console.log(`All positive: ${allPositive}`);
    
    // 对象getter/setter测试
    console.log(`Full name: ${person.fullName}`);
    person.fullName = "Jane Smith";
    console.log(`Updated full name: ${person.fullName}`);
    
    // JSON操作测试
    console.log(`JSON string: ${jsonString}`);
    console.log(`Parsed data:`, parsedData);
    
    // 正则表达式测试
    console.log(`Is valid email: ${isValidEmail('test@example.com')}`);
    
    // 错误处理测试
    console.log(`Divide 10 by 2: ${divide(10, 2)}`);
    console.log(`Divide 10 by 0: ${divide(10, 0)}`);
    
    // 模块模式测试
    console.log(`Circle area: ${MathModule.circleArea(5)}`);
    console.log(`Circle circumference: ${MathModule.circleCircumference(5)}`);
    
    // 代理测试
    proxyObject.test = "Hello";
    console.log(`Proxy value: ${proxyObject.test}`);
}

// 立即执行函数表达式 (IIFE)
(function() {
    console.log("IIFE executed immediately");
})();

// 导出函数（CommonJS风格）
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        main,
        Dog,
        add,
        multiply,
        Calculator,
        MathModule
    };
}

// 运行主函数
if (typeof require === 'undefined') {
    main();
}