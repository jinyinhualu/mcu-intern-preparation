# AddCMul 学习进度日志

这份文件持续记录 AddCMul 的实际学习进度、练习结果、易错点和下一步。概念说明见 `addcmul-quickstart.md`，题目与实现说明见 `../problems/addcmul.md`。

## 2026-09-10：广播、stride 与单元素完整计算

### 当前进度

已经掌握：

1. 从右向左对齐输入 shape。
2. 按广播规则推导输出 shape。
3. 把一维输出下标还原为多维输出坐标。
4. 根据广播维度，把输出坐标映射到每个输入的坐标。
5. 根据原始 shape 计算连续内存的 stride。
6. 用“坐标乘 stride 后求和”计算输入的一维 offset。
7. 对一个输出元素完整执行 AddCMul 公式。
8. 完成输出 Tensor 的第一个切片 `y[0,:,:]`。
9. 完成并校正 shape 为 `[2,3,4]` 的完整输出 Tensor。

暂未开始：

- 独立编写朴素 CPU 版本。
- Ascend C 代码、tiling、UB 搬运和多核切分。

### AddCMul 公式

```text
y = input_data + value * x1 * x2
```

对任意一个输出位置：

```text
y[outIndex]
    = input_data[inputOffset]
    + value[0] * x1[x1Offset] * x2[x2Offset]
```

`input_data`、`x1`、`x2` 可以广播；`value` 是 shape 为 `[1]` 的标量 Tensor。

### 广播坐标的固定方法

以输出 shape `[2,3,4]`、输入 `x2` shape `[3,1]` 为例：

```text
x2 原始 shape：   [  3,1]
x2 对齐后 shape： [1,3,1]
输出坐标：        [1,2,3]
```

逐维处理：

- 输入维度等于输出维度：保留输出坐标。
- 输入维度是 `1`：该维坐标改为 `0`。

所以：

```text
x2 对齐坐标：[0,2,0]
x2 原始坐标：[2,0]
```

使用原始 shape `[3,1]` 展平：

```text
offset = 2 * 1 + 0 = 2
x2[2] = 30
```

关键认识：广播不是在内存中复制数据，而是让多个输出坐标访问同一个输入元素。

### stride 的计算

连续内存中，某一维的 stride 表示该维坐标增加 `1` 时，一维下标要前进多少个元素。

对 shape `[d0,d1,...,dn]`：

```text
最后一维 stride = 1
前一维 stride   = 右边所有维度大小的乘积
```

例 1：

```text
shape  = [2,3,5]
stride = [3*5,5,1]
       = [15,5,1]

坐标 [1,2,3] 的 offset：
1*15 + 2*5 + 3*1 = 28
```

例 2：

```text
shape  = [2,3,4,5]
stride = [3*4*5,4*5,5,1]
       = [60,20,5,1]

坐标 [1,2,3,4] 的 offset：
1*60 + 2*20 + 3*5 + 4*1 = 119
```

### 已纠正的 stride 易错点

shape `[2,3,4,5]` 的正确 stride 是：

```text
[60,20,5,1]
```

第三维的 stride 是右侧维度 `5`，不是当前维度 `4`。因此不能写成 `[60,20,4,1]`。

### 广播与 stride 的组合

可以用两种等价方法计算广播输入的 offset。

方法一：先把广播维度的坐标改成 `0`，再乘普通 stride。

```text
输入对齐 shape：[1,3,1]
普通 stride：   [3,1,1]
输出坐标：      [1,2,3]
输入坐标：      [0,2,0]
offset：        0*3 + 2*1 + 0*1 = 2
```

方法二：保留输出坐标，但把广播维度的有效 stride 设为 `0`。

```text
输入对齐 shape：[1,3,1]
有效 stride：   [0,1,0]
输出坐标：      [1,2,3]
offset：        1*0 + 2*1 + 3*0 = 2
```

当前练习主要采用方法一。两种方法结果必须一致。

注意：计算某个输入的 offset 时，必须使用该输入自己的 stride，不能误用输出 Tensor 的 stride。

### 完整练习数据

```text
input_data shape = [2,1,4]
input_data =
[
  [[10,11,12,13]],
  [[20,21,22,23]]
]

x1 shape = [1,3,1]
x1 =
[
  [[2],
   [3],
   [4]]
]

x2 shape = [3,4]
x2 =
[
  [1, 2, 3, 4],
  [5, 6, 7, 8],
  [9,10,11,12]
]

value = 0.5
```

右对齐后的 shape：

```text
input_data：[2,1,4]
x1：        [1,3,1]
x2：        [1,3,4]
output：    [2,3,4]
```

输出一共包含：

```text
2 * 3 * 4 = 24 个元素
```

### 对 outIndex = 23 的完整计算

输出 shape `[2,3,4]` 的 stride 是 `[12,4,1]`：

```text
outIndex = 23
输出坐标 = [1,2,3]
```

三个输入的坐标和 offset：

```text
input_data 对齐 shape：[2,1,4]
input_data 坐标：     [1,0,3]
input_data stride：   [4,4,1]
inputOffset：         1*4 + 0*4 + 3*1 = 7

x1 对齐 shape：[1,3,1]
x1 坐标：     [0,2,0]
x1 stride：   [3,1,1]
x1Offset：    0*3 + 2*1 + 0*1 = 2

x2 对齐 shape：[1,3,4]
x2 坐标：     [0,2,3]
x2 stride：   [12,4,1]
x2Offset：    0*12 + 2*4 + 3*1 = 11
```

取值并代入公式：

```text
input_data[7] = 23
x1[2]         = 4
x2[11]        = 12

x1 * x2             = 4 * 12 = 48
value * x1 * x2     = 0.5 * 48 = 24
y[23]               = 23 + 24 = 47
```

这个单元素计算结果已经确认正确。

### 完整输出 Tensor

```text
y[0,:,:] =
[
  [11,   13, 15,   17],
  [17.5, 20, 22.5, 25],
  [28,   31, 34,   37]
]

y[1,:,:] =
[
  [21,   23, 25,   27],
  [27.5, 30, 32.5, 35],
  [38,   41, 44,   47]
]
```

本次纠正的易错点：

```text
1.0 * [1,2,3,4] = [1,2,3,4]
```

标量 `1.0` 要乘 `x2` 的每个元素，不能把整行误写成 `[1,1,1,1]`。这里的乘法和加法全部是逐元素运算。

例如第二个切片第一行：

```text
[20,21,22,23] + 1.0 * [1,2,3,4]
= [20,21,22,23] + [1,2,3,4]
= [21,23,25,27]
```

### 当前形成的完整逻辑链

```text
输入 shapes
    -> 右对齐并检查广播
    -> 得到输出 shape
    -> 用输出 stride 将 outIndex 还原为输出坐标
    -> 按各输入的广播维度映射输入坐标
    -> 用各输入自己的 stride 计算 offset
    -> 读取三个输入值和 value
    -> 执行 input_data + value*x1*x2
    -> 写入 y[outIndex]
```

### 下一步

备赛目标不是无限练习手算，而是完整解决第一道题。接下来按以下顺序推进：

1. 只再做一次完整输出 Tensor 手算，闭合 AddCMul 的语义和广播逻辑。
2. 不看现成模拟器，先独立写出“同 shape、单循环”的 CPU 最小版本。
3. 在最小版本上加入通用广播 offset，形成 CPU 正确性基准。
4. 覆盖同 shape、低维广播、高维广播、非整块尾部、空 Tensor 和各 dtype 测试。
5. 对照 CPU 基准拆解 Host tiling 与 Device kernel，把已有模板从“代码草案”变成自己能解释和修改的实现。
6. 在具备 CANN 的 Linux/昇腾环境中编译、运行或提交，根据真实报错继续修正。
7. 通过测试或 OJ 后完成题目复盘，AddCMul 才算真正闭环。

完整 Tensor 手算已经结束。当前任务切换为：独立编写不支持广播、三个输入 shape 相同的 CPU 最小实现。

### 第一题的完成标准

以下条件全部满足，才把 AddCMul 标记为“已解决”：

- 能独立说明公式、shape 推导、广播坐标和 offset。
- 能从空白开始写出 CPU 正确性实现，而不是只阅读现成代码。
- 关键边界测试全部通过。
- 能说明 Host 侧和 Device 侧每一部分为何存在。
- Ascend C 工程在真实 CANN 环境编译成功，并通过运行验证或 OJ。
- 日志记录最终实现、遇到的错误和验证结果。

仓库中当前已有 CPU 模拟器和 Ascend C 模板实现，但它们不能替代上述学习与验证。尤其是 Ascend C 模板尚未在真实 CANN 环境编译运行，因此当前状态仍是“AddCMul 学习与实现中”。

## 2026-09-10：CPU 最小版本

### 已完成

在三个输入 shape 相同、数据连续且类型为 `float` 的前提下，已经写出 AddCMul 的核心循环：

```cpp
int i = 0;
while (i < totalNum)
{
    y[i] = inputData[i] + x1[i] * x2[i] * value;
    i++;
}
```

使用测试数据：

```text
inputData = [1,2,3]
x1        = [2,3,4]
x2        = [3,4,5]
value     = 0.5
```

手动验证结果：

```text
y[0] = 1 + 2*3*0.5 = 4
y[1] = 2 + 3*4*0.5 = 8
y[2] = 3 + 4*5*0.5 = 13
```

### 已纠正的问题

不能写成：

```cpp
while (i++ < 3)
```

因为进入第一次循环体时 `i` 已经变成 `1`，最终还会访问 `y[3]`，造成越界。同时循环次数不能写死为 `3`，应使用参数 `totalNum`。

### 下一步

已经创建完整程序 `tools/addcmul_cpu_basic.cpp`，其中包含测试数据、函数调用、结果打印和自动检查。

本机验证命令：

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -static-libgcc -static-libstdc++ tools\addcmul_cpu_basic.cpp -o addcmul_cpu_basic.exe
.\addcmul_cpu_basic.exe
```

实际输出：

```text
y = [4, 8, 13]
Test passed.
```

第一次使用 MinGW 动态链接版本运行时出现 Windows 访问异常；加入 `-static-libgcc -static-libstdc++` 后正常运行。这属于本机 MinGW 动态运行库环境问题，不是 AddCMul 代码越界。

CPU 最小版本已经通过本地验证。下一步先读懂完整程序中参数、数组和循环的对应关系，然后在 CPU 版本中加入第一种广播场景。

## 2026-09-10：检查实际测试空工程

已只读检查：

```text
C:\Users\HP\Desktop\Addcmul_problem_6_template\code
```

确认这份模板只提供构建、注册、dtype 模板分发和 kernel 入口骨架。`InferShape`、`InferDataType`、Kernel `Init`、Kernel `Process` 均未实现；tiling 目前只传入 `input_data` 的元素数，也没有广播信息、tile 信息和多核切分信息。

详细逐文件分析见 `notes/addcmul-empty-template-analysis.md`。

当前下一步调整为：先理解 Host 中的 `tiling->length` 如何传到 Kernel 的 `Init(length)`，再设计第一版单 core、同 shape、float32 实现。CPU 广播版本暂缓到理解这条 Host-Device 参数链之后。

## 2026-09-11：开始阅读真实空工程

学习方式调整为逐文件阅读，不一次性跳入完整实现。第一步查看原始工程根目录的 `CMakeLists.txt`。

### 根目录 CMakeLists.txt 的作用

它不是 AddCMul 的计算代码，而是 CANN 工程的构建说明书，负责告诉 CMake：

```text
使用什么构建工具
目标是哪一类昇腾计算单元
最终算子包叫什么名字
Host 子工程和 Kernel 子工程在哪里
```

根文件中的核心关系：

```text
根 CMakeLists.txt
    ├── add_subdirectory(op_host)
    └── add_subdirectory(op_kernel)
```

这一阶段已理解：CMake 负责组织和构建，不负责执行 `input_data + x1*x2*value`；真正的算子公式在后续 Device Kernel 中实现。

下一步：继续阅读 `op_host/CMakeLists.txt`，理解 Host 源码如何生成 tiling 动态库和算子调用库。
