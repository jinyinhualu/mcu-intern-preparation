# AddCMul 模板导读

模板目录：

```text
problems/Addcmul_problem_6_template/
```

## 先看哪几个文件

| 文件 | 作用 | 先看重点 |
| --- | --- | --- |
| `code/op_host/addcmul.cpp` | Host 侧注册、shape 推导、tiling | `InferShape`、`TilingFunc` |
| `code/op_kernel/addcmul_tiling.h` | Host 传给 Device 的参数结构 | `AddcmulTilingData` |
| `code/op_kernel/addcmul.cpp` | Device 侧 kernel | `Init`、`Process`、`SplitCoreRange` |
| `code/op_kernel/tiling_key_addcmul.h` | dtype 到模板参数的映射 | `C_DT_FLOAT16`、`C_DT_FLOAT`、`C_DT_INT8`、`C_DT_INT32` |

## 当前实现分成两条路径

### 1. 同形状浮点向量路径

触发条件：

```text
input_data、x1、x2 展开后元素数都等于输出元素数
并且 dtype 是 float16 或 float32
```

执行位置：

```text
code/op_kernel/addcmul.cpp
ProcessVectorSameShape()
```

数据流：

```text
GM -> UB -> Mul -> Muls -> Add -> GM
```

这条路径体现 Ascend C element-wise 算子的标准写法：`DataCopyPad` 搬入 UB，调用向量 API 计算，再写回 GM。

### 2. 广播/整数逐元素路径

触发条件：

```text
存在广播
或者 dtype 是 int8 / int32
```

执行位置：

```text
code/op_kernel/addcmul.cpp
ProcessScalar()
```

这条路径用 `CalcBroadcastOffset` 根据输出线性下标计算每个输入的真实 offset。它的优势是语义直接、容易验证；缺点是性能不如向量路径。

## Host 侧最重要的三个动作

### 推导输出形状

```text
InferShape -> CalcBroadcastShape
```

按 NumPy/PyTorch 广播规则，从右往左对齐维度。

### 准备 stride

```text
FillAlignedShapeAndStride
```

把每个输入补齐到输出 rank，并计算连续 ND Tensor 的 stride。Device 侧广播访问靠这些 stride 找输入 offset。

### 设置启动核数

```text
TilingFunc -> blockDim
```

当前策略是每个 core 至少期望处理约 256 个元素，小数据少开核，大数据最多用平台给出的 AIV core 数。

## Device 侧最重要的三个动作

### 切当前 core 的范围

```text
SplitCoreRange()
```

把 `totalLength` 平均分给 `blockDim` 个 core，前面的若干 core 多处理 1 个元素。

### 处理尾块

```text
curLen = min(TILE_LENGTH, remain)
```

每个 tile 都按真实 `curLen` 搬运、计算和写回，所以 `[3,4,19]` 这类非对齐 shape 不会按完整 tile 越界。

### 处理广播 offset

```text
CalcBroadcastOffset()
```

如果输入在某一维是 `1`，这一维不参与 offset；否则用输出坐标乘以该维 stride。

## 继续优化的方向

1. 给 `int32` 加同形状向量路径，但要确认 OJ 的整数溢出语义。
2. 给常见广播加向量 fast path，例如 `[B,N]` 与 `[1,N]`。
3. 根据 UB 大小动态计算 `TILE_LENGTH`。
4. 如果 CANNJudge 对 rank 超过 8 有测试，把 `ADDCMUL_MAX_DIMS` 调大并确认 tiling buffer 大小。
5. 在 NPU 环境中编译运行后，根据报错微调 API 签名和 tiling 结构。
