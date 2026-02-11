/* C 测试文件 - 包含各种C特性 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 结构体定义
typedef struct {
    int id;
    char name[50];
    float score;
} Student;

struct Point {
    int x;
    int y;
};

// 联合体定义
union Data {
    int i;
    float f;
    char str[20];
};

// 枚举定义
enum Weekday {
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY
};

// 函数声明
void printStudent(Student s);
int calculateSum(int arr[], int size);
void swap(int *a, int *b);

// 宏定义
#define MAX_SIZE 100
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))

// 全局变量
static int globalCounter = 0;
const float PI = 3.14159;

// 函数指针类型定义
typedef int (*OperationFunc)(int, int);

// 结构体函数指针
typedef struct {
    char operation;
    OperationFunc func;
} Operation;

// 具体函数实现
int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

// 函数指针数组
Operation operations[] = {
    {'+', add},
    {'-', subtract},
    {'*', multiply}
};

// 递归函数
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// 变参函数
int sum(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }
    
    va_end(args);
    return total;
}

// 文件操作函数
void fileOperations() {
    FILE *file = fopen("test.txt", "w");
    if (file != NULL) {
        fprintf(file, "C Language Test\n");
        fclose(file);
    }
}

// 内存管理函数
void memoryManagement() {
    int *arr = (int*)malloc(10 * sizeof(int));
    if (arr != NULL) {
        for (int i = 0; i < 10; i++) {
            arr[i] = i * i;
        }
        free(arr);
    }
}

// 字符串处理函数
void stringOperations() {
    char str1[50] = "Hello";
    char str2[50] = "World";
    char result[100];
    
    strcpy(result, str1);
    strcat(result, " ");
    strcat(result, str2);
    
    printf("%s\n", result);
}

// 主函数
int main() {
    printf("C Multilingual Test\n");
    
    // 结构体测试
    Student student1 = {1, "Alice", 95.5};
    printStudent(student1);
    
    // 枚举测试
    enum Weekday today = WEDNESDAY;
    printf("Today is day %d of the week\n", today);
    
    // 函数指针测试
    OperationFunc op = add;
    printf("10 + 5 = %d\n", op(10, 5));
    
    // 宏测试
    printf("Square of 7 = %d\n", SQUARE(7));
    printf("Min of 15 and 8 = %d\n", MIN(15, 8));
    
    // 递归测试
    printf("Factorial of 5 = %d\n", factorial(5));
    
    // 变参函数测试
    printf("Sum of 1, 2, 3, 4, 5 = %d\n", sum(5, 1, 2, 3, 4, 5));
    
    return 0;
}

// 函数定义
void printStudent(Student s) {
    printf("Student: ID=%d, Name=%s, Score=%.2f\n", s.id, s.name, s.score);
}

int calculateSum(int arr[], int size) {
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum;
}

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}