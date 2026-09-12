# AddCMul 真实空模板剖析

分析日期：2026-09-10

原始模板位置：

```text
C:\Users\HP\Desktop\Addcmul_problem_6_template\code
```

分析原则：桌面原始模板保持不变，只把结论记录到学习仓库。

## 总体结论

这个模板已经提供了“算子工程外壳”，但几乎没有提供 AddCMul 的实际实现。

已经提供：

- CMake 构建和算子打包框架。
- 四个输入、一个输出的算子注册信息。
- 四种 dtype 的模板分发声明。
- Host tiling 函数、shape 推导、dtype 推导的函数入口。
- Device kernel 入口和 `Init`、`Process` 类结构。

仍需实现：

- 广播后的输出 shape 推导。
- 输出 dtype 推导。
- 广播所需的 shape、stride 和输出总元素数。
- 多核任务切分和尾块信息。
- GM Tensor 绑定、GM 与 UB 之间的数据搬运。
- `x1 * x2 * value + input_data` 的实际计算。
- 结果写回 GM。
- 广播、空 Tensor 和多 dtype 的边界处理。

所以这不是“在空白处填一行公式”的题，而是一道需要补齐 Host 和 Device 两侧的完整自定义算子题。

## 整个工程的调用链

```text
CMake 构建并打包算子
        |
        v
OpDef 注册输入、输出、dtype 和芯片类型
        |
        v
InferShape / InferDataType 推导输出描述
        |
        v
TilingFunc 在 Host 侧计算 kernel 所需参数
        |
        v
AddcmulTilingData 传入 Device
        |
        v
addcmul kernel 入口读取 tiling 数据
        |
        v
KernelAddcmul::Init 绑定 GM、准备 UB 和队列
        |
        v
KernelAddcmul::Process 搬入、计算、写回
```

## 根目录 CMakeLists.txt

文件：`code/CMakeLists.txt`

关键内容：

```cmake
find_package(ASC REQUIRED)
set(ASCEND_COMPUTE_UNIT ascend910b)
add_subdirectory(op_host)
add_subdirectory(op_kernel)
```

作用：

- `find_package(ASC REQUIRED)`：寻找 Ascend C/CANN 构建环境。因此普通 Windows C++ 环境不能直接构建这个工程。
- `ASCEND_COMPUTE_UNIT ascend910b`：目标计算单元是 Ascend 910B。
- `npu_op_package`：生成可以安装或运行的自定义算子包。
- 两个 `add_subdirectory`：分别构建 Host 和 Kernel 部分。

这一文件基本属于平台模板，当前不需要为了实现公式而修改。

## op_host/CMakeLists.txt

它负责把 `op_host/*.cpp` 用在三个地方：

1. 根据算子定义生成调用代码。
2. 构建 tiling 动态库 `cust_optiling`。
3. 构建 ACLNN 单算子调用库 `cust_opapi`。

这也是平台构建骨架，通常不需要改。

## op_kernel/CMakeLists.txt

它把 `op_kernel` 目录中的 Ascend C kernel 构建为 `ascendc_kernels`，并关联 Host 侧的 `cust_optiling`。

这说明 Host 写入的 tiling 数据和 Kernel 读取的 tiling 数据必须严格使用同一个结构定义。

## op_host/addcmul.cpp

这个文件同时承担三项职责：

```text
TilingFunc     -> 计算 Device 需要的运行参数
InferShape     -> 推导输出 shape
InferDataType  -> 推导输出 dtype
OpDef          -> 注册算子接口
```

### 已经提供的 OpDef

`OpDef` 已经声明：

```text
输入：input_data、x1、x2、value
输出：y
dtype：float16、float32、int8、int32
format：ND
芯片：ascend910b
```

这一段基本可以保留。它描述的是“算子长什么样”，不是“算子怎么算”。

### InferShape 目前是空实现

原模板只是：

```cpp
static graphStatus InferShape(gert::InferShapeContext *context) {
    return GRAPH_SUCCESS;
}
```

它没有给 `y` 设置任何维度。AddCMul 支持广播，因此这里必须读取前三个输入 shape，右对齐各维并推导广播后的输出 shape。

### InferDataType 目前是空实现

原模板只是返回成功，没有设置输出 dtype。至少需要让 `y` 的 dtype 跟随 `input_data`。

### TilingFunc 目前只是示例

它已经演示了如何获得：

- AIV 核数。
- UB 大小。
- 四个输入 Tensor。
- 输入 dtype、元素字节数、元素个数和内存大小。
- `AddcmulTilingData` 指针。

但真正写入 tiling 的只有：

```cpp
tiling->length = length_input_data;
```

这对完整 AddCMul 不够，原因如下：

1. 广播后输出元素数可能大于 `input_data` 的元素数。
2. 没有传递输出 shape。
3. 没有传递三个输入对齐后的 shape 和 stride。
4. 没有传递每个 core 的工作范围或计算工作范围所需的信息。
5. 读取到的 `ub_size`、`x1`、`x2`、`value`、`dtype_size` 当前都没有实际使用。

模板还直接执行：

```cpp
context->SetBlockDim(num_cores_aiv);
```

这会启动平台提供的全部 AIV core。如果 Kernel 只是让每个 core 从下标 `0` 循环到 `length`，所有 core 就会重复计算同一区域并同时写同一输出。必须加入基于 `GetBlockIdx()` 的多核区间切分，或者第一版暂时只启动一个 core。

## op_kernel/addcmul_tiling.h

当前结构只有：

```cpp
struct AddcmulTilingData {
    uint32_t length;
};
```

它就是 Host 和 Device 之间的“参数包”。

第一版同 shape 实现至少需要输出总元素数。通用广播版本还需要：

```text
rank
output shape
三个输入右对齐后的 shape
三个输入的 stride
多核切分信息
tile 长度或尾块处理信息
```

Host 写入的字段顺序、类型和数组长度，必须与 Kernel 读取的定义完全一致。

## op_kernel/tiling_key_addcmul.h

这个文件已经声明 `DT_INPUT_DATA` 可以被实例化为：

```text
float16
float32
int8
int32
```

Host 侧：

```cpp
ASCENDC_TPL_SEL_PARAM(context, DT_INPUT_DATA);
```

Kernel 侧：

```cpp
template <typename DT_INPUT_DATA>
```

二者配合，使同一个 kernel 源码可以根据输入 dtype 生成不同模板实例。第一阶段通常不需要修改这个文件。

## op_kernel/addcmul.cpp

### kernel 入口已经搭好

入口函数已经完成三件事：

```cpp
REGISTER_TILING_DEFAULT(AddcmulTilingData);
GET_TILING_DATA_WITH_STRUCT(AddcmulTilingData, tiling_data, tiling);
KernelAddcmul<DT_INPUT_DATA> op;
```

随后调用：

```cpp
op.Init(..., tiling_data.length);
op.Process();
```

这部分展示了标准结构：入口函数尽量薄，实际状态和逻辑封装在 `KernelAddcmul` 类中。

### Init 完全为空

`Init` 至少要负责：

- 保存元素总数和当前 core 的处理范围。
- 把五个 `GM_ADDR` 转为对应类型的 `GlobalTensor`。
- 读取 `value[0]`。
- 初始化 `TPipe`、输入输出队列和临时 buffer。

### Process 完全为空

`Process` 最终要形成：

```text
CopyIn：input_data、x1、x2 从 GM 搬到 UB
Compute：Mul -> Muls -> Add
CopyOut：y 从 UB 写回 GM
```

对同 shape 浮点版本，计算可以直接表达为：

```text
tmp = x1 * x2
tmp = tmp * value
y   = input_data + tmp
```

对广播版本，输出下标必须先映射成每个输入自己的 offset，不能再假设四个 Tensor 的下标都相同。

### private 区域完全为空

后续通常需要添加：

- `GlobalTensor`：四个输入和一个输出。
- `TPipe`。
- 输入、输出 `TQue`。
- 中间计算 `TBuf`。
- `totalLength`、`coreStart`、`coreLength` 等标量。
- 广播需要的 shape 和 stride 数组。

## 不能直接照搬 CPU 循环的原因

CPU 最小版本是：

```cpp
for (int i = 0; i < totalNum; ++i) {
    y[i] = inputData[i] + x1[i] * x2[i] * value;
}
```

它解决了公式，但 NPU kernel 还必须解决：

```text
哪个 core 处理这个 i？
i 对应每个广播输入的哪个 offset？
数据何时从 GM 搬到 UB？
一次搬多少才能放进 UB？
最后不足一个 tile 时处理多少？
不同 dtype 用什么中间类型和向量 API？
```

所以 CPU 版本不是无用的简化，它是 NPU 实现的正确性基准；Kernel 只是给这个循环增加并行切分、广播寻址和内存搬运。

## 当前模板的风险清单

1. `InferShape` 没有设置输出 shape。
2. `InferDataType` 没有设置输出 dtype。
3. `length` 取自 `input_data`，不是广播后输出元素数。
4. `length` 使用 `uint32_t`，大 Tensor 存在范围风险。
5. 启动全部 AIV core，但没有 core 间任务切分。
6. `value` 已获取但没有读取其第一个元素。
7. UB 大小已获取但没有用于计算 tile。
8. Kernel 没有绑定 GM Tensor。
9. Kernel 没有分配 UB、搬运、计算或写回。
10. 没有广播、尾块、空 Tensor和整数中间溢出处理。

## 备赛中的实现顺序

不要同时攻克所有问题，按可验证阶段推进：

### 阶段 1：单 core、同 shape、float32

- `InferShape` 暂时复制 `input_data` shape。
- `InferDataType` 让输出跟随输入。
- tiling 只传 `totalLength`。
- `SetBlockDim(1)`，先排除多核切分错误。
- Kernel 完成 GM 绑定和正确公式。

目标：先让最小 case 在真实 CANN 环境输出正确。

### 阶段 2：tile 与尾块

- 根据 UB 容量确定一次处理的元素数。
- 加入 `CopyIn -> Compute -> CopyOut`。
- 每轮使用真实 `curLen` 处理尾块。

### 阶段 3：多核切分

- 使用 `GetBlockIdx()` 确定当前 core。
- 计算 `coreStart` 和 `coreLength`。
- 验证输出区域不重叠、不遗漏。

### 阶段 4：广播

- Host 推导输出 shape。
- 对齐三个输入 shape 并计算 stride。
- Device 把输出下标映射为三个输入 offset。
- 先保证正确，再添加常见广播的向量 fast path。

### 阶段 5：全部 dtype 与边界

- float16 精度。
- int8、int32 中间计算范围和转换语义。
- 空 Tensor。
- 高维广播和大尺寸输入。

## 下一次讲解起点

下一步先不填所有代码，而是从 `op_kernel/addcmul.cpp` 的入口向上追踪一次参数：

```text
Host 的 tiling->length
    -> tiling buffer
    -> GET_TILING_DATA_WITH_STRUCT
    -> tiling_data.length
    -> KernelAddcmul::Init 的 length 参数
```

理解这条链后，再把 CPU 版的 `totalNum` 与模板里的 `length` 对应起来，完成第一版 `Init` 的成员设计。
