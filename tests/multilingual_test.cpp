// C++ 测试文件 - 包含各种C++特性
#include <iostream>
#include <vector>
#include <memory>

// 前向声明
class TestClass;

// 模板类声明
template<typename T>
class TemplateClass {
private:
    T value;
public:
    TemplateClass(T val) : value(val) {}
    T getValue() const { return value; }
};

// 基类
class BaseClass {
protected:
    int protectedVar;
public:
    BaseClass() : protectedVar(0) {}
    virtual ~BaseClass() = default;
    virtual void virtualMethod() = 0;
};

// 派生类
class DerivedClass : public BaseClass {
private:
    std::string name;
public:
    DerivedClass(const std::string& n) : name(n) {}
    void virtualMethod() override {
        std::cout << "Derived: " << name << std::endl;
    }
    
    // 静态方法
    static void staticMethod() {
        std::cout << "Static method" << std::endl;
    }
    
    // 友元函数
    friend void friendFunction(const DerivedClass& obj);
};

// 友元函数实现
void friendFunction(const DerivedClass& obj) {
    std::cout << "Friend: " << obj.name << std::endl;
}

// 结构体
typedef struct {
    int x;
    int y;
} Point;

struct Rectangle {
    Point topLeft;
    Point bottomRight;
    
    int area() const {
        return (bottomRight.x - topLeft.x) * (bottomRight.y - topLeft.y);
    }
};

// 枚举
enum Color {
    RED,
    GREEN,
    BLUE
};

enum class Status {
    PENDING,
    ACTIVE,
    COMPLETED
};

// 命名空间
namespace MyNamespace {
    void namespaceFunction() {
        std::cout << "Namespace function" << std::endl;
    }
    
    class NamespaceClass {
    public:
        void method() {}
    };
}

// 函数模板
template<typename T>
T max(T a, T b) {
    return (a > b) ? a : b;
}

// Lambda函数
auto lambda = []() { return 42; };

// 普通函数
void globalFunction() {
    std::cout << "Global function" << std::endl;
}

int add(int a, int b) {
    return a + b;
}

// 主函数
int main() {
    std::cout << "C++ Multilingual Test" << std::endl;
    
    DerivedClass obj("Test");
    obj.virtualMethod();
    DerivedClass::staticMethod();
    
    globalFunction();
    std::cout << "Add: " << add(5, 3) << std::endl;
    std::cout << "Max: " << max(10, 20) << std::endl;
    std::cout << "Lambda: " << lambda() << std::endl;
    
    return 0;
}