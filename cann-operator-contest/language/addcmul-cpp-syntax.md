# Addcmul 算子工程中的 C++ 语法

本文结合以下空工程整理：

~~~text
C:/Users/HP/Desktop/Addcmul_problem_6_template/code
~~~

该工程是一个 Ascend C 自定义算子模板。代码分为两部分：

- op_host：主机侧代码，负责算子注册、Shape/数据类型推导和 Tiling 参数准备。
- op_kernel：AI Core 侧代码，负责真正的数据计算。

> 当前模板中的 Init()、Process() 和 Shape 推导函数基本为空，本文重点讲解其中已经出现的 C++ 语法。

## 1. 工程文件关系

~~~text
CMakeLists.txt
    ├── op_host/addcmul.cpp
    ├── op_host/CMakeLists.txt
    ├── op_kernel/addcmul.cpp
    ├── op_kernel/addcmul_tiling.h
    ├── op_kernel/tiling_key_addcmul.h
    └── op_kernel/CMakeLists.txt
~~~

典型流程：

~~~text
Host 侧 TilingFunc
    ↓
计算长度、数据类型、核数等参数
    ↓
写入 AddcmulTilingData
    ↓
Kernel 侧读取 Tiling 数据
    ↓
创建 KernelAddcmul<具体数据类型>
    ↓
执行 Init() 和 Process()
~~~

通常 addcmul 的计算可以抽象为：

~~~cpp
y[i] = input_data[i] + value[i] * x1[i] * x2[i];
~~~

当前空工程还没有实现这个计算。

## 2. 头文件和预处理指令

Host 侧源码中：

~~~cpp
#include "register/op_def_registry.h"
#include "tiling/platform/platform_ascendc.h"
#include "../op_kernel/addcmul_tiling.h"
~~~

#include 是预处理指令，作用是把其他头文件的内容引入当前文件。#include <cstdint> 提供 uint32_t、uint64_t 等固定宽度整数类型。

头文件中：

~~~cpp
#pragma once
~~~

用于避免同一个头文件被重复包含。传统写法是：

~~~cpp
#ifndef ADDCMUL_TILING_H
#define ADDCMUL_TILING_H

// 头文件内容

#endif
~~~

## 3. 命名空间 namespace

Host 侧使用了：

~~~cpp
namespace optiling {
    static ge::graphStatus TilingFunc(...) {
        return ge::GRAPH_SUCCESS;
    }
}
~~~

命名空间用于组织代码，避免不同模块出现同名函数或变量冲突。

:: 是作用域解析运算符：

~~~cpp
ge::GRAPH_SUCCESS
optiling::TilingFunc
platform_ascendc::PlatformAscendC
~~~

它们分别表示从 ge、optiling、platform_ascendc 命名空间中查找对应名称。

简单例子：

~~~cpp
namespace math_a {
    int add(int a, int b) { return a + b; }
}

namespace math_b {
    int add(int a, int b) { return a - b; }
}

math_a::add(3, 2);  // 5
math_b::add(3, 2);  // 1
~~~

## 4. 函数定义和 static

Tiling 函数的定义是：

~~~cpp
static ge::graphStatus TilingFunc(gert::TilingContext *context) {
    // 函数体
    return ge::GRAPH_SUCCESS;
}
~~~

可以拆成：

~~~text
static                  // 当前源文件内部可见
ge::graphStatus         // 返回值类型
TilingFunc              // 函数名
gert::TilingContext*    // 参数类型
context                 // 参数名
~~~

普通函数的基本形式是：

~~~cpp
返回类型 函数名(参数列表) {
    // 函数体
}
~~~

在 .cpp 文件的全局函数前加 static，通常表示该函数只在当前源文件内部可使用，避免暴露给其他源文件。

## 5. 指针、const、. 和 ->

函数参数：

~~~cpp
gert::TilingContext *context
~~~

表示 context 是一个指针，保存某个 TilingContext 对象的地址。

指针调用成员函数时使用 ->：

~~~cpp
context->GetPlatformInfo();
~~~

它大致等价于：

~~~cpp
(*context).GetPlatformInfo();
~~~

访问规则：

~~~cpp
Student student;
student.name;       // 普通对象使用 .

Student *p = &student;
p->name;            // 对象指针使用 ->
~~~

Host 侧还使用了：

~~~cpp
const gert::Tensor *tensor_input_data;
~~~

它表示“指向只读 Tensor 对象的指针”。通过这个指针可以读取对象，但不能直接修改对象内容。这里的 const 修饰的是指向的对象，而不是指针本身。

## 6. 引用和输出参数

这段代码：

~~~cpp
uint64_t ub_size;
platform.GetCoreMemSize(
    platform_ascendc::CoreMemType::UB,
    ub_size
);
~~~

ub_size 可能作为输出参数传给函数。C++ 中常用引用实现这种效果：

~~~cpp
void getSize(uint64_t &size) {
    size = 1024;
}

uint64_t value;
getSize(value);
~~~

函数内部修改 size，外部的 value 也会改变。指针和引用的常见区别是：

~~~cpp
void f1(int *p);  // 传地址，可能为空
void f2(int &r);  // 传引用，通常必须绑定到有效对象
~~~

## 7. auto 类型自动推导

代码中：

~~~cpp
auto platform =
    platform_ascendc::PlatformAscendC(
        context->GetPlatformInfo()
    );
~~~

auto 让编译器根据右侧表达式自动推导变量类型，效果大致等价于：

~~~cpp
platform_ascendc::PlatformAscendC platform(
    context->GetPlatformInfo()
);
~~~

要注意 const 和引用可能不会自动保留：

~~~cpp
const int value = 10;
auto x = value;   // x 是 int
auto &y = value;  // y 是 const int 的引用
~~~

## 8. 固定宽度整数类型

工程中常见：

~~~cpp
int32_t
uint32_t
uint64_t
size_t
~~~

| 类型 | 含义 |
|---|---|
| int32_t | 有符号 32 位整数 |
| uint32_t | 无符号 32 位整数 |
| uint64_t | 无符号 64 位整数 |
| size_t | 表示大小、长度或下标的类型 |

例如：

~~~cpp
uint32_t length_input_data =
    tensor_input_data->GetShapeSize();
~~~

元素个数不会是负数，所以常使用无符号整数。注意有符号数和无符号数混合计算时可能出现意外结果。

## 9. static_cast 类型转换

代码中：

~~~cpp
uint32_t DT_INPUT_DATA =
    static_cast<uint32_t>(dtype_input_data);
~~~

这里将 ge::DataType 显式转换为 uint32_t。

通用形式：

~~~cpp
目标类型 result = static_cast<目标类型>(表达式);
~~~

例如：

~~~cpp
double a = 3.14;
int b = static_cast<int>(a);  // b 为 3
~~~

相比 C 风格的 (int)a，static_cast 更清晰，也更容易被编译器检查。

## 10. struct 结构体

addcmul_tiling.h 中定义了：

~~~cpp
struct AddcmulTilingData {
    uint32_t length;
};
~~~

它用于保存 Host 侧传给 Kernel 侧的参数。

普通对象访问成员：

~~~cpp
AddcmulTilingData data;
data.length = 100;
~~~

指针访问成员：

~~~cpp
AddcmulTilingData *tiling;
tiling->length = 100;
~~~

Host 侧对应代码：

~~~cpp
AddcmulTilingData *tiling =
    context->GetTilingData<AddcmulTilingData>();

tiling->length = length_input_data;
~~~

## 11. 模板 template

Kernel 中：

~~~cpp
template <class DT_INPUT_DATA>
class KernelAddcmul {
    // ...
};
~~~

这表示 KernelAddcmul 是一个类模板，DT_INPUT_DATA 是类型参数。

标准 C++ 示例：

~~~cpp
template <typename T>
T add(T a, T b) {
    return a + b;
}

add<int>(1, 2);
add<float>(1.0f, 2.0f);
~~~

class 和 typename 在模板参数声明中基本等价：

~~~cpp
template <class T>
class A {};

template <typename T>
class B {};
~~~

本工程的模板参数用于支持 DT_FLOAT16、DT_FLOAT、DT_INT8 和 DT_INT32，即根据不同数据类型生成不同的 Kernel 版本。

## 12. 类、继承和访问权限

Host 侧定义了：

~~~cpp
class Addcmul : public OpDef {
public:
    explicit Addcmul(const char *name) : OpDef(name) {
        // 算子配置
    }
};
~~~

class Addcmul 定义了一个类。: public OpDef 表示 Addcmul 公有继承自 OpDef，因此可以使用父类提供的公有接口，例如：

~~~cpp
this->Input(...)
this->Output(...)
this->AICore()
~~~

访问权限：

~~~cpp
public:
    // 外部可以访问

private:
    // 只能由类内部访问
~~~

Kernel 类中的 Init() 和 Process() 位于 public 区域，所以 Kernel 入口可以调用：

~~~cpp
op.Init(...);
op.Process();
~~~

## 13. 构造函数、初始化列表和 explicit

构造函数：

~~~cpp
explicit Addcmul(const char *name) : OpDef(name) {
    // 构造函数体
}
~~~

构造函数名称必须和类名相同，并且没有返回值类型。冒号后的 : OpDef(name) 叫构造函数初始化列表，表示创建 Addcmul 对象时先调用父类 OpDef 的构造函数。

初始化成员变量时也常使用初始化列表：

~~~cpp
class Buffer {
public:
    Buffer(int n) : size(n) {}

private:
    int size;
};
~~~

explicit 用来禁止不希望出现的隐式转换：

~~~cpp
class Number {
public:
    explicit Number(int value) {}
};

Number a(10);      // 可以
Number b = 10;     // 不允许隐式转换
~~~

## 14. this 指针

代码中：

~~~cpp
this->Input("input_data")
~~~

this 是指向当前对象的指针，表示调用当前 Addcmul 对象的 Input 成员函数。通常也可以写成：

~~~cpp
Input("input_data")
~~~

## 15. 花括号初始化列表

算子配置中：

~~~cpp
.DataType({
    ge::DT_FLOAT16,
    ge::DT_FLOAT,
    ge::DT_INT8,
    ge::DT_INT32
})
~~~

花括号是 C++ 初始化列表语法，常用于初始化数组、容器或框架接口参数。

标准 C++ 示例：

~~~cpp
int values[] = {1, 2, 3, 4};
~~~

这里的 .DataType({...}) 是框架 API，但 {...} 本身是标准 C++ 语法。

## 16. 链式调用

输入配置使用了链式调用：

~~~cpp
this->Input("input_data")
    .ParamType(REQUIRED)
    .DataType({...})
    .Format({...});
~~~

这通常要求每个成员函数返回当前对象的引用：

~~~cpp
class Config {
public:
    Config &setA(int value) {
        a = value;
        return *this;
    }

    Config &setB(int value) {
        b = value;
        return *this;
    }

private:
    int a = 0;
    int b = 0;
};

Config config;
config.setA(1).setB(2);
~~~

在算子工程中，链式调用用于连续配置输入、输出和执行方式。

## 17. 对象创建和成员函数调用

Kernel 入口中：

~~~cpp
KernelAddcmul<DT_INPUT_DATA> op;
~~~

表示按照 DT_INPUT_DATA 这个类型，创建一个模板类对象。如果 DT_INPUT_DATA 等于 float，它相当于：

~~~cpp
KernelAddcmul<float> op;
~~~

之后：

~~~cpp
op.Init(...);
op.Process();
~~~

因为 op 是普通对象，所以使用 .；如果 op 是指针，则应使用 ->。

## 18. inline 和 Ascend C 修饰符

代码中：

~~~cpp
__aicore__ inline void Process() {
}
~~~

inline 是标准 C++ 关键字，表示允许编译器尝试将函数调用展开，以减少函数调用开销。它只是建议，是否展开由编译器决定。

下面这些不是标准 C++，而是 Ascend C 或编译器扩展：

~~~cpp
__aicore__
__global__
GM_ADDR
~~~

常见含义：

- __aicore__：函数运行在 AI Core 上。
- __global__：函数是可以被调度的 Kernel 入口。
- GM_ADDR：框架定义的全局内存地址类型。

因此：

~~~cpp
template <typename DT_INPUT_DATA>
__global__ __aicore__ void addcmul(...)
~~~

可以理解为“定义一个支持不同数据类型的 AI Core Kernel 入口函数”。

## 19. 宏

Kernel 入口中出现：

~~~cpp
REGISTER_TILING_DEFAULT(AddcmulTilingData);
GET_TILING_DATA_WITH_STRUCT(
    AddcmulTilingData,
    tiling_data,
    tiling
);
~~~

这些是宏，由预处理器在编译前展开，不是普通的 C++ 函数调用。

宏的简单例子：

~~~cpp
#define SQUARE(x) ((x) * (x))

int y = SQUARE(3);
~~~

Ascend C 框架宏的具体实现位于框架头文件中。这里重点理解它们的作用：读取和解析 Tiling 数据。

## 20. Host 与 Kernel 的参数传递

Host 侧先计算输入长度：

~~~cpp
uint32_t length_input_data =
    tensor_input_data->GetShapeSize();
~~~

然后写入 Tiling 结构体：

~~~cpp
AddcmulTilingData *tiling =
    context->GetTilingData<AddcmulTilingData>();

tiling->length = length_input_data;
~~~

Kernel 侧读取：

~~~cpp
GET_TILING_DATA_WITH_STRUCT(
    AddcmulTilingData,
    tiling_data,
    tiling
);
~~~

再传给算子对象：

~~~cpp
op.Init(
    input_data,
    x1,
    x2,
    value,
    y,
    tiling_data.length
);
~~~

数据流：

~~~text
length_input_data
    → tiling->length
    → tiling_data.length
    → Init(..., length)
~~~

## 21. 状态码和 return

函数最后返回：

~~~cpp
return ge::GRAPH_SUCCESS;
~~~

表示当前操作成功。

Shape 推导函数目前是：

~~~cpp
static graphStatus InferShape(gert::InferShapeContext *context) {
    return GRAPH_SUCCESS;
}
~~~

这只是模板占位代码，实际项目中应该根据输入 Tensor 的形状设置输出 Tensor 的形状。

## 22. Tiling Key 模板宏

在 tiling_key_addcmul.h 中：

~~~cpp
ASCENDC_TPL_ARGS_DECL(Addcmul,
    ASCENDC_TPL_DATATYPE_DECL(
        DT_INPUT_DATA,
        C_DT_FLOAT16,
        C_DT_FLOAT,
        C_DT_INT8,
        C_DT_INT32
    ),
);
~~~

这部分是 Ascend C 框架提供的模板宏，用于声明和选择不同的数据类型版本。它背后的目的类似标准 C++ 模板：根据输入数据类型选择对应的 Kernel 实现。

Host 侧：

~~~cpp
uint32_t DT_INPUT_DATA =
    static_cast<uint32_t>(dtype_input_data);

ASCENDC_TPL_SEL_PARAM(context, DT_INPUT_DATA);
~~~

Kernel 侧：

~~~cpp
KernelAddcmul<DT_INPUT_DATA> op;
~~~

两者共同完成“Host 确定类型，Kernel 使用类型”的过程。

## 23. C++ 语法与框架语法的区分

### 标准 C++ 语法

~~~cpp
#include
#pragma once
namespace
static
const
auto
static_cast
struct
class
template
public
private
explicit
this
.
->
&
*
return
~~~

### Ascend C 或算子框架扩展

~~~cpp
__aicore__
__global__
GM_ADDR
REGISTER_TILING_DEFAULT
GET_TILING_DATA_WITH_STRUCT
ASCENDC_TPL_ARGS_DECL
ASCENDC_TPL_SEL
OP_ADD
OpDef
TilingContext
~~~

学习时应先掌握标准 C++ 语法，再理解这些框架接口。否则容易把宏、编译器修饰符和普通 C++ 语法混在一起。

## 24. 建议学习顺序

1. 掌握变量、函数、结构体、指针和引用。
2. 掌握 const、.、->、&、*。
3. 掌握类、构造函数、继承、访问权限和 this。
4. 掌握模板 template <typename T>。
5. 掌握初始化列表、花括号初始化和链式调用。
6. 掌握 auto、static_cast 和固定宽度整数类型。
7. 了解宏、inline 和编译器扩展。
8. 最后学习 Ascend C 的 Tiling、GlobalTensor、LocalTensor 和 Kernel 调度。

当前工程中最值得重点理解的一句是：

~~~cpp
KernelAddcmul<DT_INPUT_DATA> op;
~~~

它表示“按照 DT_INPUT_DATA 这个数据类型，创建一个对应版本的 KernelAddcmul 对象”。这是 C++ 模板语法与硬件算子编程结合的核心。
