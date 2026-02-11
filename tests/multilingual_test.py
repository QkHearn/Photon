# Python 测试文件 - 包含各种Python特性

import asyncio
from typing import List, Dict, Optional, Union
from dataclasses import dataclass
from abc import ABC, abstractmethod
import json

# 数据类装饰器
@dataclass
class Person:
    name: str
    age: int
    email: str = ""

# 普通类定义
class Animal:
    """动物基类"""
    
    def __init__(self, name: str, species: str):
        self.name = name
        self.species = species
    
    def speak(self) -> str:
        return f"{self.name} makes a sound"

# 抽象类
class AbstractVehicle(ABC):
    @abstractmethod
    def start_engine(self) -> str:
        pass
    
    @abstractmethod
    def stop_engine(self) -> str:
        pass

# 继承类
class Car(AbstractVehicle):
    def __init__(self, brand: str, model: str):
        self.brand = brand
        self.model = model
        self._engine_running = False
    
    def start_engine(self) -> str:
        self._engine_running = True
        return f"{self.brand} {self.model} engine started"
    
    def stop_engine(self) -> str:
        self._engine_running = False
        return f"{self.brand} {self.model} engine stopped"

# 枚举类（Python 3.4+）
from enum import Enum, auto

class Color(Enum):
    RED = auto()
    GREEN = auto()
    BLUE = auto()

class Status(Enum):
    PENDING = "pending"
    ACTIVE = "active"
    COMPLETED = "completed"

# 函数定义
def greet(name: str) -> str:
    """简单的问候函数"""
    return f"Hello, {name}!"

def calculate_area(length: float, width: float) -> float:
    """计算矩形面积"""
    return length * width

def process_data(data: List[Dict[str, Union[str, int]]]) -> Dict[str, int]:
    """处理数据列表"""
    result = {}
    for item in data:
        key = item.get("category", "unknown")
        value = item.get("value", 0)
        result[key] = result.get(key, 0) + value
    return result

# 异步函数
async def fetch_data(url: str) -> Dict:
    """模拟异步数据获取"""
    await asyncio.sleep(1)  # 模拟网络延迟
    return {"url": url, "data": "sample data"}

async def main_async():
    """主异步函数"""
    result = await fetch_data("https://api.example.com/data")
    return result

# 生成器函数
def fibonacci_generator(n: int):
    """斐波那契数列生成器"""
    a, b = 0, 1
    for _ in range(n):
        yield a
        a, b = b, a + b

# Lambda函数
square = lambda x: x ** 2
cube = lambda x: x ** 3

# 高阶函数
def create_multiplier(factor: int):
    """创建乘法器函数"""
    def multiplier(x: int) -> int:
        return x * factor
    return multiplier

times_three = create_multiplier(3)

# 上下文管理器
class FileManager:
    """文件上下文管理器"""
    
    def __init__(self, filename: str, mode: str):
        self.filename = filename
        self.mode = mode
        self.file = None
    
    def __enter__(self):
        self.file = open(self.filename, self.mode)
        return self.file
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.file:
            self.file.close()

# 属性装饰器
class Circle:
    def __init__(self, radius: float):
        self._radius = radius
    
    @property
    def radius(self) -> float:
        return self._radius
    
    @radius.setter
    def radius(self, value: float):
        if value > 0:
            self._radius = value
    
    @property
    def area(self) -> float:
        return 3.14159 * self._radius ** 2

# 静态方法和类方法
class MathUtils:
    PI = 3.14159
    
    @staticmethod
    def add(x: float, y: float) -> float:
        return x + y
    
    @classmethod
    def get_pi(cls) -> float:
        return cls.PI

# 异常处理
def safe_divide(a: float, b: float) -> float:
    """安全的除法运算"""
    try:
        return a / b
    except ZeroDivisionError:
        print("Error: Division by zero")
        return 0.0
    except TypeError:
        print("Error: Invalid type")
        return 0.0

# 列表推导式
def process_numbers(numbers: List[int]) -> List[int]:
    """使用列表推导式处理数字"""
    return [x ** 2 for x in numbers if x > 0]

# 字典推导式
def create_square_dict(n: int) -> Dict[int, int]:
    """创建平方数字典"""
    return {i: i ** 2 for i in range(1, n + 1)}

# 主函数
def main():
    print("Python Multilingual Test")
    
    # 基本函数测试
    print(greet("World"))
    print(f"Area: {calculate_area(5.0, 3.0)}")
    
    # 类测试
    car = Car("Toyota", "Camry")
    print(car.start_engine())
    print(car.stop_engine())
    
    # 枚举测试
    print(f"Color: {Color.RED.name}")
    print(f"Status: {Status.ACTIVE.value}")
    
    # 生成器测试
    fib_sequence = list(fibonacci_generator(10))
    print(f"Fibonacci: {fib_sequence}")
    
    # Lambda测试
    print(f"Square of 5: {square(5)}")
    print(f"Cube of 3: {cube(3)}")
    
    # 高阶函数测试
    print(f"Times three of 4: {times_three(4)}")
    
    # 数据处理测试
    sample_data = [
        {"category": "A", "value": 10},
        {"category": "B", "value": 20},
        {"category": "A", "value": 15}
    ]
    result = process_data(sample_data)
    print(f"Processed data: {result}")
    
    # 列表推导式测试
    numbers = [1, 2, 3, 4, 5, -1, -2]
    squared = process_numbers(numbers)
    print(f"Squared positive numbers: {squared}")
    
    # 字典推导式测试
    square_dict = create_square_dict(5)
    print(f"Square dictionary: {square_dict}")
    
    # 属性测试
    circle = Circle(5.0)
    print(f"Circle radius: {circle.radius}, area: {circle.area}")
    
    # 静态方法测试
    print(f"MathUtils.add(10, 20): {MathUtils.add(10, 20)}")
    print(f"MathUtils.get_pi(): {MathUtils.get_pi()}")
    
    # 异常处理测试
    result = safe_divide(10, 0)
    print(f"Safe divide result: {result}")

# 数据类实例
person1 = Person("Alice", 30, "alice@example.com")

# 运行主函数
if __name__ == "__main__":
    main()